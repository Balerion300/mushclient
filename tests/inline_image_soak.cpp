// Long-running, isolated loader soak. Compile with the loopback-only test flag.
#include "stdafx.h"
#include "InlineImage.h"

#include <psapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <memory>
#include <vector>

#pragma comment(lib, "psapi.lib")

namespace
{
  struct Sample
  {
    SIZE_T privateBytes;
    DWORD gdiHandles;
  };

  Sample Measure ()
  {
    PROCESS_MEMORY_COUNTERS_EX memory = {};
    memory.cb = sizeof memory;
    Sample result = {};
    if (GetProcessMemoryInfo (GetCurrentProcess (),
          reinterpret_cast<PROCESS_MEMORY_COUNTERS*> (&memory), sizeof memory))
      result.privateBytes = memory.PrivateUsage;
    result.gdiHandles = GetGuiResources (GetCurrentProcess (), GR_GDIOBJECTS);
    return result;
  }

  bool WaitReady (const std::vector<std::shared_ptr<CInlineImage> >& images,
                  DWORD timeoutMs)
  {
    const DWORD start = GetTickCount ();
    for (;;)
      {
        bool pending = false;
        for (size_t i = 0; i < images.size (); ++i)
          {
            const CInlineImage::State state = images[i]->GetState ();
            if (state == CInlineImage::Failed) return false;
            if (state == CInlineImage::Loading) pending = true;
          }
        if (!pending) return true;
        if (GetTickCount () - start >= timeoutMs) return false;
        Sleep (10);
      }
  }

  unsigned CountExpired (const std::vector<std::weak_ptr<CInlineImage> >& refs)
  {
    unsigned count = 0;
    for (size_t i = 0; i < refs.size (); ++i)
      if (refs[i].expired ()) ++count;
    return count;
  }

  void Row (FILE* output, DWORD elapsedMs, unsigned cycle, const char* phase,
            const Sample& sample, unsigned images, unsigned expired)
  {
    fprintf (output, "%lu,%u,%s,%llu,%lu,%u,%u\n",
             static_cast<unsigned long> (elapsedMs), cycle, phase,
             static_cast<unsigned long long> (sample.privateBytes),
             static_cast<unsigned long> (sample.gdiHandles), images, expired);
    fflush (output);
  }
}

int main (int argc, char** argv)
{
  if (argc != 4)
    { fprintf (stderr, "usage: inline_image_soak BASE_URL SECONDS CSV_PATH\n"); return 2; }
  if (!AfxWinInit (GetModuleHandle (NULL), NULL, GetCommandLine (), 0))
    { fprintf (stderr, "MFC initialization failed\n"); return 2; }
  const long durationSeconds = strtol (argv[2], NULL, 10);
  if (durationSeconds < 10 || durationSeconds > 3600)
    { fprintf (stderr, "duration must be 10..3600 seconds\n"); return 2; }
  FILE* output = NULL;
  if (fopen_s (&output, argv[3], "wb") || !output)
    { fprintf (stderr, "cannot open CSV output\n"); return 2; }
  fprintf (output, "elapsed_ms,cycle,phase,private_bytes,gdi_handles,images,expired\n");

  CString base (argv[1]);
  std::shared_ptr<CInlineImage> pinned = CInlineImage::Request (base + _T("/png?pinned=1"));
  std::vector<std::shared_ptr<CInlineImage> > pinList (1, pinned);
  if (!WaitReady (pinList, 15000))
    { fprintf (stderr, "pinned fixture failed\n"); fclose (output); return 1; }
  const DWORD start = GetTickCount ();
  unsigned cycle = 0;
  unsigned requests = 0;
  bool failed = false;
  while (GetTickCount () - start < static_cast<DWORD> (durationSeconds * 1000))
    {
      const DWORD cycleStart = GetTickCount ();
      std::vector<std::shared_ptr<CInlineImage> > images;
      std::vector<std::weak_ptr<CInlineImage> > weak;
      images.reserve (16);
      weak.reserve (16);
      for (unsigned i = 0; i < 16; ++i)
        {
          CString url;
          url.Format (_T("%s/scene?soak=%u"), static_cast<LPCTSTR> (base), requests++);
          std::shared_ptr<CInlineImage> image = CInlineImage::Request (url);
          if (!image) { failed = true; break; }
          weak.push_back (image);
          images.push_back (image);
        }
      if (failed || !WaitReady (images, 15000))
        { fprintf (stderr, "image failed or timed out in cycle %u\n", cycle); failed = true; break; }
      Row (output, GetTickCount () - start, cycle, "held", Measure (),
           static_cast<unsigned> (images.size ()), CountExpired (weak));
      images.clear ();
      const DWORD releaseStart = GetTickCount ();
      while (CountExpired (weak) != weak.size () &&
             GetTickCount () - releaseStart < 5000)
        Sleep (10);
      const unsigned expired = CountExpired (weak);
      Row (output, GetTickCount () - start, cycle, "released", Measure (),
           static_cast<unsigned> (weak.size ()), expired);
      if (expired != weak.size ())
        { fprintf (stderr, "unreleased image in cycle %u: %u/16\n", cycle, expired); failed = true; break; }
      ++cycle;
      const DWORD spent = GetTickCount () - cycleStart;
      if (spent < 2000) Sleep (2000 - spent);
      if (cycle % 30 == 0)
        { printf ("cycles=%u requests=%u elapsed_s=%lu\n", cycle, requests,
                  static_cast<unsigned long> ((GetTickCount () - start) / 1000)); fflush (stdout); }
    }
  fclose (output);
  printf ("SOAK %s cycles=%u requests=%u elapsed_s=%lu\n", failed ? "FAIL" : "PASS",
          cycle, requests, static_cast<unsigned long> ((GetTickCount () - start) / 1000));
  return failed ? 1 : 0;
}
