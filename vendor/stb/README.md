# stb

Single-file public-domain libraries. https://github.com/nothings/stb

- **File:** `stb_truetype.h` v1.26
- **File:** `stb_image.h` v2.30 (sha256 `594c2fe3…200b3`)
- **License:** public domain (or MIT, see notice in the header)

`stb_truetype.h` is used by `packages/glim-paint` to bake a Latin SDF atlas (K14).
`stb_image.h` is used by `packages/glim-image` to decode JPEG reference images
for probe comparisons (`STBI_ONLY_JPEG`, `STBI_ONLY_PNG`). Do not add other
third-party trees under `packages/`.
