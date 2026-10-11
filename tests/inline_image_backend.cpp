// Native integration test for InlineImage.cpp. Build this executable with
// MUSHCLIENT_INLINE_IMAGE_TEST_ALLOW_LOOPBACK; the application must omit it.
// The HTTP fixture server is tests/inline_image_fixture.py.
#include "stdafx.h"
#include "InlineImage.h"

#include <stdio.h>
#include <memory>
#include <vector>

namespace
{
  int failures = 0;

  void Check (bool condition, const char* description)
  {
    printf ("%s %s\n", condition ? "PASS" : "FAIL", description);
    if (!condition) ++failures;
  }

  CString Address (const CString& base, const char* path)
  {
    CString url (base);
    url += path;
    return url;
  }

  CInlineImage::State WaitFor (const std::shared_ptr<CInlineImage>& image,
                               DWORD timeoutMs = 15000)
  {
    const DWORD started = GetTickCount ();
    while (image->GetState () == CInlineImage::Loading &&
           GetTickCount () - started < timeoutMs)
      Sleep (10);
    return image->GetState ();
  }

  COLORREF CenterPixel (const std::shared_ptr<CInlineImage>& image,
                        COLORREF background)
  {
    BITMAPINFO info = {};
    info.bmiHeader.biSize = sizeof (BITMAPINFOHEADER);
    info.bmiHeader.biWidth = 8;
    info.bmiHeader.biHeight = -8;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    DWORD* pixels = NULL;
    HBITMAP bitmap = CreateDIBSection (NULL, &info, DIB_RGB_COLORS,
                                       reinterpret_cast<void**> (&pixels),
                                       NULL, 0);
    if (!bitmap || !pixels) return CLR_INVALID;
    const DWORD bg = 0xff000000 |
                     GetBValue (background) |
                     (GetGValue (background) << 8) |
                     (GetRValue (background) << 16);
    for (int i = 0; i < 64; ++i) pixels[i] = bg;
    HDC memory = CreateCompatibleDC (NULL);
    if (!memory)
      { DeleteObject (bitmap); return CLR_INVALID; }
    HGDIOBJ previous = SelectObject (memory, bitmap);
    CDC dc;
    dc.Attach (memory);
    image->Draw (&dc, CRect (0, 0, 8, 8));
    dc.Detach ();
    GdiFlush ();
    DWORD pixel = pixels[4 + 4 * 8];
    SelectObject (memory, previous);
    DeleteDC (memory);
    DeleteObject (bitmap);
    return RGB ((pixel >> 16) & 0xff, (pixel >> 8) & 0xff, pixel & 0xff);
  }

  bool Near (COLORREF actual, COLORREF expected, int tolerance = 3)
  {
    return abs (static_cast<int> (GetRValue (actual)) - GetRValue (expected)) <= tolerance &&
           abs (static_cast<int> (GetGValue (actual)) - GetGValue (expected)) <= tolerance &&
           abs (static_cast<int> (GetBValue (actual)) - GetBValue (expected)) <= tolerance;
  }
}

int main (int argc, char** argv)
{
  if (argc != 2)
    {
    fprintf (stderr, "usage: inline_image_backend.exe http://localhost:PORT\n");
    return 2;
    }
  if (!AfxWinInit (GetModuleHandle (NULL), NULL, GetCommandLine (), 0))
    return 3;

  CString base (argv[1]);
  const DWORD before = GetTickCount ();
  std::shared_ptr<CInlineImage> slow = CInlineImage::Request (Address (base, "/slow"));
  Check (GetTickCount () - before < 250, "Request returns without waiting for HTTP");
  Check (CInlineImage::Request (Address (base, "/slow")) == slow,
         "same URL shares the in-flight image");

  const char* formats[] = { "/png", "/jpeg", "/gif", "/bmp" };
  for (int i = 0; i < 4; ++i)
    {
    std::shared_ptr<CInlineImage> image = CInlineImage::Request (Address (base, formats[i]));
    Check (WaitFor (image) == CInlineImage::Ready, formats[i]);
    Check (image->GetWidth () == 2 && image->GetHeight () == 2,
           "decoded image reports fixture dimensions");
    }

  std::shared_ptr<CInlineImage> gif = CInlineImage::Request (Address (base, "/gif"));
  Check (WaitFor (gif) == CInlineImage::Ready, "GIF is ready before drawing");
  Check (Near (CenterPixel (gif, RGB (255,255,255)), RGB (255,0,0)),
         "animated GIF draws its red first frame");

  std::shared_ptr<CInlineImage> alpha = CInlineImage::Request (Address (base, "/alpha"));
  Check (WaitFor (alpha) == CInlineImage::Ready, "transparent PNG decodes");
  Check (Near (CenterPixel (alpha, RGB (255,255,255)), RGB (255,127,127)),
         "premultiplied alpha blends over white");

  std::shared_ptr<CInlineImage> redirect = CInlineImage::Request (Address (base, "/redirect"));
  Check (WaitFor (redirect) == CInlineImage::Ready, "HTTP redirect follows valid target");
  std::shared_ptr<CInlineImage> unicode =
    CInlineImage::Request (Address (base, "/caf\xC3\xA9"));
  Check (WaitFor (unicode) == CInlineImage::Ready,
         "UTF-8 source path is percent encoded for HTTP");

  const char* failuresToTry[] = { "/bad", "/too-large", "/huge-dimensions",
                                   "/redirect-file", "/redirect-loop" };
  for (int i = 0; i < 5; ++i)
    {
    std::shared_ptr<CInlineImage> image = CInlineImage::Request (Address (base, failuresToTry[i]));
    Check (WaitFor (image) == CInlineImage::Failed, failuresToTry[i]);
    Check (!image->GetError ().IsEmpty (), "failed image has an explanation");
    }

  std::shared_ptr<CInlineImage> file = CInlineImage::Request ("file:///C:/Windows/win.ini");
  Check (WaitFor (file) == CInlineImage::Failed, "local file URL is rejected");
  std::shared_ptr<CInlineImage> otherLoopback =
    CInlineImage::Request ("http://127.0.0.2/image.png");
  Check (WaitFor (otherLoopback) == CInlineImage::Failed,
         "test hook only permits the exact fixture loopback host");

  Check (WaitFor (slow) == CInlineImage::Ready, "slow download eventually completes");
  slow.reset ();

  std::shared_ptr<CInlineImage> large1 =
    CInlineImage::Request (Address (base, "/large?i=1"));
  std::shared_ptr<CInlineImage> large2 =
    CInlineImage::Request (Address (base, "/large?i=2"));
  Check (WaitFor (large1, 30000) == CInlineImage::Ready &&
         WaitFor (large2, 30000) == CInlineImage::Ready,
         "two large images fit the global bitmap budget");
  std::shared_ptr<CInlineImage> large3 =
    CInlineImage::Request (Address (base, "/large?i=3"));
  Check (WaitFor (large3, 30000) == CInlineImage::Failed,
         "third large image is rejected at the global bitmap limit");
  large1.reset ();
  std::shared_ptr<CInlineImage> large4 =
    CInlineImage::Request (Address (base, "/large?i=4"));
  Check (WaitFor (large4, 30000) == CInlineImage::Ready,
         "dropping an image releases its bitmap budget");

  std::shared_ptr<CInlineImage> discarded =
    CInlineImage::Request (Address (base, "/slow?discarded=1"));
  std::weak_ptr<CInlineImage> abandoned = discarded;
  discarded.reset ();
  // The worker owns the request until completion, then its last reference
  // disappears without a document or view being present.
  const DWORD discardedAt = GetTickCount ();
  while (!abandoned.expired () && GetTickCount () - discardedAt < 5000)
    Sleep (10);
  Check (abandoned.expired (),
         "dropping a view reference during download releases the image safely");

  printf ("%d failure(s)\n", failures);
  return failures ? 1 : 0;
}
