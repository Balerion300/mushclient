#pragma once

#include <afxwin.h>
#include <atomic>
#include <memory>
#include <string>

// An image received from a remote server. Request only queues work; callers
// can poll GetState from the UI thread and invalidate their view when ready.
// The image owns no document or window pointers.
class CInlineImage
{
public:
  enum State { Loading, Ready, Failed };

  static std::shared_ptr<CInlineImage> Request (const CString& source);

  ~CInlineImage ();
  State GetState () const;
  int GetWidth () const;
  int GetHeight () const;
  CString GetError () const;
  void Draw (CDC* dc, const CRect& rect) const;

private:
  friend class InlineImageCache;
  explicit CInlineImage (const std::wstring& source);
  CInlineImage (const CInlineImage&) = delete;
  CInlineImage& operator= (const CInlineImage&) = delete;

  void Load ();
  void Fail (const char* message);

  std::wstring source_;
  std::atomic<State> state_;
  HBITMAP bitmap_;
  int width_;
  int height_;
  size_t bitmapBytes_;
  CString error_;
};
