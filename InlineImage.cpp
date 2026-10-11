#include "stdafx.h"
#include "InlineImage.h"

#include <winhttp.h>
#include <wincodec.h>
#include <shlwapi.h>
#include <atomic>
#include <condition_variable>
#include <deque>
#include <exception>
#include <map>
#include <mutex>
#include <thread>
#include <vector>

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "msimg32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "normaliz.lib")

namespace
{
  const size_t kMaxDownload = 16 * 1024 * 1024;
  const size_t kMaxBitmapBytes = 128 * 1024 * 1024;
  const size_t kMaxBitmaps = 512;
  const size_t kMaxPixels = 16 * 1024 * 1024;
  const unsigned int kMaxDimension = 8192;
  const size_t kMaxQueued = 64;
  const size_t kMaxCacheKeys = 256;
  const DWORD kTotalDownloadMs = 25000;
  std::atomic<size_t> gBitmapBytes (0);
  std::atomic<size_t> gBitmapCount (0);

  struct InternetHandle
  {
    HINTERNET value;
    explicit InternetHandle (HINTERNET handle = NULL) : value (handle) {}
    ~InternetHandle () { if (value) WinHttpCloseHandle (value); }
    InternetHandle (const InternetHandle&) = delete;
    InternetHandle& operator= (const InternetHandle&) = delete;
  };

  template<class T> struct ComPtr
  {
    T* value;
    ComPtr () : value (NULL) {}
    ~ComPtr () { if (value) value->Release (); }
    T** Out () { return &value; }
    T* operator-> () const { return value; }
    operator bool () const { return value != NULL; }
    ComPtr (const ComPtr&) = delete;
    ComPtr& operator= (const ComPtr&) = delete;
  };

  bool ReserveBitmapBytes (size_t bytes)
  {
    size_t old = gBitmapBytes.load (std::memory_order_relaxed);
    do
      {
      if (bytes > kMaxBitmapBytes - old)
        return false;
      }
    while (!gBitmapBytes.compare_exchange_weak (old, old + bytes,
                                                std::memory_order_acq_rel,
                                                std::memory_order_relaxed));
    return true;
  }

  std::wstring WideSource (const CString& source)
  {
#ifdef _UNICODE
    return std::wstring (source.GetString (), source.GetLength ());
#else
    const char* bytes = source.GetString ();
    const int length = source.GetLength ();
    if (length <= 0)
      return std::wstring ();
    // MXP is received as bytes in this MBCS build. UTF-8 URLs retain their
    // characters; older ACP-encoded URLs remain usable as a fallback.
    UINT encoding = CP_UTF8;
    int count = MultiByteToWideChar (encoding, MB_ERR_INVALID_CHARS,
                                     bytes, length, NULL, 0);
    if (!count)
      {
      encoding = CP_ACP;
      count = MultiByteToWideChar (encoding, 0, bytes, length, NULL, 0);
      }
    if (!count)
      return std::wstring ();
    std::wstring result (count, L'\0');
    MultiByteToWideChar (encoding, encoding == CP_UTF8 ? MB_ERR_INVALID_CHARS : 0,
                         bytes, length, &result[0], count);
    return result;
#endif
  }

  bool IsLocalHost (const std::wstring& host)
  {
    if (host.empty ()) return true;
    std::wstring lower (host);
    std::transform (lower.begin (), lower.end (), lower.begin (), towlower);
#ifdef MUSHCLIENT_INLINE_IMAGE_TEST_ALLOW_LOOPBACK
    // Only a separately compiled integration test may reach its fixture
    // server. This switch must never be set in the application project.
    if (lower == L"localhost" || lower == L"127.0.0.1" ||
        lower == L"::1" || lower == L"[::1]")
      return false;
#endif
    if (lower == L"localhost" || lower == L"localhost." ||
        lower.size () >= 10 && lower.compare (lower.size () - 10, 10, L".localhost") == 0 ||
        lower.size () >= 6 && lower.compare (lower.size () - 6, 6, L".local") == 0)
      return true;
    // Server-provided image references must not address a local service by
    // literal IP or single-label host. Public DNS names remain supported.
    if (lower.find (L'.') == std::wstring::npos ||
        lower.find (L':') != std::wstring::npos)
      return true;
    bool numeric = true;
    for (size_t i = 0; i < lower.size (); ++i)
      if ((lower[i] < L'0' || lower[i] > L'9') && lower[i] != L'.')
        numeric = false;
    return numeric;
  }

  struct ParsedUrl
  {
    std::wstring host;
    std::wstring target;
    INTERNET_PORT port;
    bool secure;
  };

  bool EscapeUnicodeTarget (const std::wstring& input, std::wstring& output)
  {
    const int count = WideCharToMultiByte (CP_UTF8, WC_ERR_INVALID_CHARS,
                                           input.c_str (),
                                           static_cast<int> (input.size ()),
                                           NULL, 0, NULL, NULL);
    if (count <= 0) return false;
    std::string bytes (count, '\0');
    if (!WideCharToMultiByte (CP_UTF8, WC_ERR_INVALID_CHARS,
                              input.c_str (), static_cast<int> (input.size ()),
                              &bytes[0], count, NULL, NULL))
      return false;
    const WCHAR hex[] = L"0123456789ABCDEF";
    output.clear ();
    for (size_t i = 0; i < bytes.size (); ++i)
      {
      const unsigned char ch = static_cast<unsigned char> (bytes[i]);
      if (ch < 0x80) output.push_back (static_cast<WCHAR> (ch));
      else
        {
        output.push_back (L'%');
        output.push_back (hex[ch >> 4]);
        output.push_back (hex[ch & 15]);
        }
      }
    return output.size () <= 8192;
  }

  bool ParseRemoteUrl (const std::wstring& source, ParsedUrl& parsed)
  {
    if (source.empty () || source.size () > 2048 ||
        source.find (L'\\') != std::wstring::npos)
      return false;
    for (size_t i = 0; i < source.size (); ++i)
      if (source[i] <= 0x20 || source[i] == 0x7f)
        return false;

    URL_COMPONENTS parts = {};
    parts.dwStructSize = sizeof (parts);
    parts.dwSchemeLength = static_cast<DWORD> (-1);
    parts.dwHostNameLength = static_cast<DWORD> (-1);
    parts.dwUrlPathLength = static_cast<DWORD> (-1);
    parts.dwExtraInfoLength = static_cast<DWORD> (-1);
    parts.dwUserNameLength = static_cast<DWORD> (-1);
    parts.dwPasswordLength = static_cast<DWORD> (-1);
    if (!WinHttpCrackUrl (source.c_str (), static_cast<DWORD> (source.size ()),
                          0, &parts) ||
        (parts.nScheme != INTERNET_SCHEME_HTTP &&
         parts.nScheme != INTERNET_SCHEME_HTTPS) ||
        parts.dwUserNameLength || parts.dwPasswordLength ||
        !parts.lpszHostName || !parts.dwHostNameLength)
      return false;

    parsed.host.assign (parts.lpszHostName, parts.dwHostNameLength);
    if (parsed.host.size () > 253) return false;
    bool nonAsciiHost = false;
    for (size_t i = 0; i < parsed.host.size (); ++i)
      if (parsed.host[i] >= 0x80) nonAsciiHost = true;
    if (nonAsciiHost)
      {
      WCHAR ascii[256] = {};
      if (!IdnToAscii (IDN_USE_STD3_ASCII_RULES, parsed.host.c_str (),
                       -1, ascii, ARRAYSIZE (ascii)))
        return false;
      parsed.host.assign (ascii);
      }
    if (IsLocalHost (parsed.host))
      return false;
    std::wstring target = parts.dwUrlPathLength
      ? std::wstring (parts.lpszUrlPath, parts.dwUrlPathLength) : L"/";
    if (parts.dwExtraInfoLength)
      target.append (parts.lpszExtraInfo, parts.dwExtraInfoLength);
    if (!EscapeUnicodeTarget (target, parsed.target)) return false;
    parsed.port = parts.nPort;
    parsed.secure = parts.nScheme == INTERNET_SCHEME_HTTPS;
    return true;
  }

  bool CombineRedirect (const std::wstring& base,
                        const std::wstring& location,
                        std::wstring& result)
  {
    if (location.empty () || location.size () > 2048)
      return false;
    WCHAR combined[4096];
    DWORD length = ARRAYSIZE (combined);
    if (FAILED (UrlCombineW (base.c_str (), location.c_str (), combined,
                            &length, 0)) || length > 2048)
      return false;
    result.assign (combined, length);
    return true;
  }

  bool Download (const std::wstring& source, std::vector<BYTE>& bytes,
                 const char*& error)
  {
    InternetHandle session (WinHttpOpen (L"MUSHclient inline image/1.0",
                                          WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                          WINHTTP_NO_PROXY_NAME,
                                          WINHTTP_NO_PROXY_BYPASS, 0));
    if (!session.value || !WinHttpSetTimeouts (session.value, 8000, 8000,
                                               8000, 8000))
      { error = "Unable to initialize HTTP"; return false; }

    std::wstring current = source;
    DWORD started = GetTickCount ();
    for (unsigned int redirect = 0; redirect <= 5; ++redirect)
      {
      if (GetTickCount () - started > kTotalDownloadMs)
        { error = "Image download timed out"; return false; }
      ParsedUrl url;
      if (!ParseRemoteUrl (current, url))
        { error = "Image URL must be a public HTTP or HTTPS URL"; return false; }
      InternetHandle connection (WinHttpConnect (session.value,
                                                 url.host.c_str (), url.port, 0));
      if (!connection.value)
        { error = "Unable to connect to image host"; return false; }
      InternetHandle request (WinHttpOpenRequest (connection.value, L"GET",
                                                  url.target.c_str (), NULL,
                                                  WINHTTP_NO_REFERER,
                                                  WINHTTP_DEFAULT_ACCEPT_TYPES,
                                                  url.secure ? WINHTTP_FLAG_SECURE : 0));
      if (!request.value)
        { error = "Unable to request image"; return false; }
      DWORD noRedirect = WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
      if (!WinHttpSetOption (request.value, WINHTTP_OPTION_REDIRECT_POLICY,
                             &noRedirect, sizeof (noRedirect)))
        { error = "Unable to enforce redirect policy"; return false; }
      DWORD disabled = WINHTTP_DISABLE_COOKIES | WINHTTP_DISABLE_AUTHENTICATION;
      WinHttpSetOption (request.value, WINHTTP_OPTION_DISABLE_FEATURE,
                        &disabled, sizeof (disabled));
      if (!WinHttpSendRequest (request.value, WINHTTP_NO_ADDITIONAL_HEADERS,
                               0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
          !WinHttpReceiveResponse (request.value, NULL))
        { error = "Image download failed"; return false; }

      DWORD status = 0, statusSize = sizeof (status);
      if (!WinHttpQueryHeaders (request.value,
                                WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                                WINHTTP_HEADER_NAME_BY_INDEX, &status,
                                &statusSize, WINHTTP_NO_HEADER_INDEX))
        { error = "Invalid HTTP response"; return false; }

      if (status >= 300 && status <= 399)
        {
        if (redirect == 5)
          { error = "Too many image redirects"; return false; }
        WCHAR location[4096] = {};
        DWORD locationSize = sizeof (location);
        if (!WinHttpQueryHeaders (request.value, WINHTTP_QUERY_LOCATION,
                                  WINHTTP_HEADER_NAME_BY_INDEX, location,
                                  &locationSize, WINHTTP_NO_HEADER_INDEX))
          { error = "Image redirect has no location"; return false; }
        std::wstring next;
        if (!CombineRedirect (current, location, next))
          { error = "Invalid image redirect"; return false; }
        ParsedUrl nextUrl;
        if (!ParseRemoteUrl (next, nextUrl) || (url.secure && !nextUrl.secure))
          { error = "Unsafe image redirect"; return false; }
        current.swap (next);
        continue;
        }

      if (status != 200)
        { error = "Image server returned an error"; return false; }
      DWORD contentLength = 0, lengthSize = sizeof (contentLength);
      if (WinHttpQueryHeaders (request.value,
                               WINHTTP_QUERY_CONTENT_LENGTH | WINHTTP_QUERY_FLAG_NUMBER,
                               WINHTTP_HEADER_NAME_BY_INDEX, &contentLength,
                               &lengthSize, WINHTTP_NO_HEADER_INDEX) &&
          contentLength > kMaxDownload)
        { error = "Image download exceeds size limit"; return false; }

      bytes.clear ();
      for (;;)
        {
        if (GetTickCount () - started > kTotalDownloadMs)
          { error = "Image download timed out"; return false; }
        BYTE chunk[8192];
        DWORD read = 0;
        if (!WinHttpReadData (request.value, chunk, sizeof (chunk), &read))
          { error = "Image download failed"; return false; }
        if (!read)
          return true;
        if (read > kMaxDownload - bytes.size ())
          { error = "Image download exceeds size limit"; return false; }
        bytes.insert (bytes.end (), chunk, chunk + read);
        }
      }
    error = "Too many image redirects";
    return false;
  }

  bool Decode (std::vector<BYTE>& bytes, HBITMAP& bitmap,
               int& width, int& height, size_t& bitmapBytes,
               const char*& error)
  {
    if (bytes.empty ())
      { error = "Empty image response"; return false; }
    ComPtr<IWICImagingFactory> factory;
    HRESULT hr = CoCreateInstance (CLSID_WICImagingFactory, NULL,
                                    CLSCTX_INPROC_SERVER,
                                    IID_PPV_ARGS (factory.Out ()));
    if (FAILED (hr))
      { error = "Windows image decoder is unavailable"; return false; }
    ComPtr<IWICStream> stream;
    hr = factory->CreateStream (stream.Out ());
    if (SUCCEEDED (hr))
      hr = stream->InitializeFromMemory (&bytes[0], static_cast<DWORD> (bytes.size ()));
    ComPtr<IWICBitmapDecoder> decoder;
    if (SUCCEEDED (hr))
      hr = factory->CreateDecoderFromStream (stream.value, NULL,
                                             WICDecodeMetadataCacheOnDemand,
                                             decoder.Out ());
    if (FAILED (hr))
      { error = "Unsupported or damaged image"; return false; }
    GUID format;
    if (FAILED (decoder->GetContainerFormat (&format)) ||
        (format != GUID_ContainerFormatPng &&
         format != GUID_ContainerFormatJpeg &&
         format != GUID_ContainerFormatGif &&
         format != GUID_ContainerFormatBmp))
      { error = "Only PNG, JPEG, GIF, and BMP images are supported"; return false; }
    ComPtr<IWICBitmapFrameDecode> frame;
    hr = decoder->GetFrame (0, frame.Out ());
    UINT w = 0, h = 0;
    if (SUCCEEDED (hr))
      hr = frame->GetSize (&w, &h);
    if (FAILED (hr) || !w || !h || w > kMaxDimension || h > kMaxDimension ||
        static_cast<size_t> (w) * h > kMaxPixels)
      { error = "Image dimensions exceed limit"; return false; }
    ComPtr<IWICFormatConverter> converter;
    hr = factory->CreateFormatConverter (converter.Out ());
    if (SUCCEEDED (hr))
      hr = converter->Initialize (frame.value, GUID_WICPixelFormat32bppPBGRA,
                                   WICBitmapDitherTypeNone, NULL, 0.0,
                                   WICBitmapPaletteTypeCustom);
    if (FAILED (hr))
      { error = "Unable to convert image pixels"; return false; }

    const size_t size = static_cast<size_t> (w) * h * 4;
    if (!ReserveBitmapBytes (size))
      { error = "Inline image memory limit reached"; return false; }
    const size_t count = gBitmapCount.fetch_add (1, std::memory_order_acq_rel);
    if (count >= kMaxBitmaps)
      {
      gBitmapCount.fetch_sub (1, std::memory_order_acq_rel);
      gBitmapBytes.fetch_sub (size, std::memory_order_acq_rel);
      error = "Inline image resource limit reached";
      return false;
      }
    BITMAPINFO info = {};
    info.bmiHeader.biSize = sizeof (BITMAPINFOHEADER);
    info.bmiHeader.biWidth = static_cast<LONG> (w);
    info.bmiHeader.biHeight = -static_cast<LONG> (h);
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    void* pixels = NULL;
    HBITMAP result = CreateDIBSection (NULL, &info, DIB_RGB_COLORS,
                                       &pixels, NULL, 0);
    if (!result || !pixels ||
        FAILED (converter->CopyPixels (NULL, w * 4,
                                       static_cast<UINT> (size),
                                       static_cast<BYTE*> (pixels))))
      {
      if (result) DeleteObject (result);
      gBitmapBytes.fetch_sub (size, std::memory_order_acq_rel);
      gBitmapCount.fetch_sub (1, std::memory_order_acq_rel);
      error = "Unable to decode image pixels";
      return false;
      }
    bitmap = result;
    width = static_cast<int> (w);
    height = static_cast<int> (h);
    bitmapBytes = size;
    return true;
  }
}

class InlineImageCache
{
public:
  InlineImageCache ()
  {
    // Deliberately process-lifetime: detached workers cannot observe a
    // destructed queue during application shutdown.
    for (int i = 0; i < 4; ++i)
      std::thread (&InlineImageCache::Worker, this).detach ();
  }

  std::shared_ptr<CInlineImage> Request (const std::wstring& source)
  {
    std::lock_guard<std::mutex> lock (mutex_);
    std::map<std::wstring, std::weak_ptr<CInlineImage> >::iterator existing =
      cache_.find (source);
    if (existing != cache_.end ())
      {
      std::shared_ptr<CInlineImage> image = existing->second.lock ();
      if (image) return image;
      cache_.erase (existing);
      }
    std::shared_ptr<CInlineImage> image (new CInlineImage (source));
    if (source.empty () || source.size () > 2048)
      { image->Fail ("Invalid image URL"); return image; }
    if (queue_.size () >= kMaxQueued)
      { image->Fail ("Too many pending image downloads"); return image; }
    if (cache_.size () >= kMaxCacheKeys)
      {
      for (std::map<std::wstring, std::weak_ptr<CInlineImage> >::iterator it = cache_.begin ();
           it != cache_.end (); )
        {
        if (it->second.expired ()) it = cache_.erase (it);
        else ++it;
        }
      if (cache_.size () >= kMaxCacheKeys)
        cache_.erase (cache_.begin ());
      }
    cache_[source] = image;
    queue_.push_back (image);
    cv_.notify_one ();
    return image;
  }

private:
  void Worker ()
  {
    HRESULT com = CoInitializeEx (NULL, COINIT_MULTITHREADED);
    for (;;)
      {
      std::shared_ptr<CInlineImage> image;
      {
        std::unique_lock<std::mutex> lock (mutex_);
        cv_.wait (lock, [this] { return !queue_.empty (); });
        image = queue_.front ();
        queue_.pop_front ();
      }
      try
        {
        if (SUCCEEDED (com)) image->Load ();
        else image->Fail ("Windows image decoder is unavailable");
        }
      catch (const std::exception&)
        { image->Fail ("Unable to allocate image resources"); }
      catch (...)
        { image->Fail ("Image processing failed"); }
      }
  }

  std::mutex mutex_;
  std::condition_variable cv_;
  std::deque<std::shared_ptr<CInlineImage> > queue_;
  std::map<std::wstring, std::weak_ptr<CInlineImage> > cache_;
};

CInlineImage::CInlineImage (const std::wstring& source)
  : source_ (source), state_ (Loading), bitmap_ (NULL), width_ (0),
    height_ (0), bitmapBytes_ (0)
{
}

CInlineImage::~CInlineImage ()
{
  if (bitmap_) DeleteObject (bitmap_);
  if (bitmapBytes_)
    {
    gBitmapBytes.fetch_sub (bitmapBytes_, std::memory_order_acq_rel);
    gBitmapCount.fetch_sub (1, std::memory_order_acq_rel);
    }
}

std::shared_ptr<CInlineImage> CInlineImage::Request (const CString& source)
{
  // The queue/cache's lifetime intentionally matches the process.
  static InlineImageCache* cache = new InlineImageCache;
  std::wstring url = WideSource (source);
  const size_t fragment = url.find (L'#');
  if (fragment != std::wstring::npos) url.erase (fragment);
  return cache->Request (url);
}

void CInlineImage::Fail (const char* message)
{
  error_ = message;
  state_.store (Failed, std::memory_order_release);
}

void CInlineImage::Load ()
{
  const char* error = "Unable to load image";
  std::vector<BYTE> bytes;
  if (!Download (source_, bytes, error) ||
      !Decode (bytes, bitmap_, width_, height_, bitmapBytes_, error))
    { Fail (error); return; }
  state_.store (Ready, std::memory_order_release);
}

CInlineImage::State CInlineImage::GetState () const
{
  return state_.load (std::memory_order_acquire);
}

int CInlineImage::GetWidth () const
{
  return GetState () == Ready ? width_ : 0;
}

int CInlineImage::GetHeight () const
{
  return GetState () == Ready ? height_ : 0;
}

CString CInlineImage::GetError () const
{
  return GetState () == Failed ? error_ : CString ();
}

void CInlineImage::Draw (CDC* dc, const CRect& rect) const
{
  if (!dc || !dc->GetSafeHdc () || GetState () != Ready ||
      rect.Width () <= 0 || rect.Height () <= 0)
    return;

  const double scale = min (static_cast<double> (rect.Width ()) / width_,
                            static_cast<double> (rect.Height ()) / height_);
  const int drawWidth = max (1, static_cast<int> (width_ * scale + 0.5));
  const int drawHeight = max (1, static_cast<int> (height_ * scale + 0.5));
  const int x = rect.left + (rect.Width () - drawWidth) / 2;
  const int y = rect.top + (rect.Height () - drawHeight) / 2;
  HDC memory = CreateCompatibleDC (dc->GetSafeHdc ());
  if (!memory) return;
  HGDIOBJ previous = SelectObject (memory, bitmap_);
  if (previous && previous != HGDI_ERROR)
    {
    BLENDFUNCTION blend = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
    AlphaBlend (dc->GetSafeHdc (), x, y, drawWidth, drawHeight,
                memory, 0, 0, width_, height_, blend);
    SelectObject (memory, previous);
    }
  DeleteDC (memory);
}
