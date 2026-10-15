# Valyria identity

The user authorized reuse of the existing Valyria client's Fourteen Flames
micro mark for this client on October 15, 2026. These files are the existing
identity assets, copied byte for byte from the installed 0.55.0 RC1 bundle's
`icons` directory. No Atlantis or Valyria implementation code is imported.

`res/valyria-brand-provenance.json` preserves the original identity-package
source, its source hash, the selected ember gradient, and the geometry record.
The mark has four spires and four detached flame diamonds. Its eight paths and
selected colors are unchanged. `valyria.svg`, `.png`, and `.ico` are transparent
application versions. `valyria-desktop.*` are the existing ivory-tile versions
for shortcuts and contexts where the transparent mark needs a background.

The native executable's main, world, and text document icon resources reference
`res/valyria.ico`. Shared runtime identifiers and filenames remain compatible;
this icon change is not a complete rename or settings migration. The prototype
uses the Valyria identity with red as its default accent and blue as an option.
No personal installation or Windows shortcut is changed by this source update.

The clean portable package includes the seven original identity files in
`icons` and this notice in `licences`. Engine licensing and upstream attribution
remain intact. Importing these branding assets does not grant a separate MIT
license to the Fourteen Flames identity or imply third-party trademark rights.

Verification: the seven imported files match the installed source byte for byte.
The Release executable built successfully. Its three icon groups each contain
the original fourteen image payloads (16 through 256 pixels), verified directly
from the compiled resources. A fictional loopback client startup and Lua
plugin/logging smoke test passed. Existing installed clients were untouched.
