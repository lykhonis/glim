# Working on Glim

## Composition first (K89)

Glim grows by small, general primitives that compose. A feature is a
recipe of existing primitives, not a new primitive.

- **Effects are compositions.** A reflection is a copy of a piece,
  flipped, faded and made inert: `replica`, `scale`, `fade`, `inert`.
  It is never a `reflect` flag. The same holds for echoes, shadows of
  content, badges, and edge fades.
- **No per-feature fields.** A new field on an element, op, paint shape
  or shell frame that serves one effect is a one-off. Find the primitive
  it is an instance of, or the two it combines.
- **One meaning per primitive, everywhere.** A primitive works on any
  content: Glim-drawn, Texture and native layers. Where a platform
  cannot honor it yet, the gap is the primitive's, documented once, not
  worked around per feature.
- **Primitives span the stack once.** Toolkit tag → op → paint group →
  renderers (software and GPU) → shells where native content is
  involved. Hit testing, focus and a11y follow paint's geometry through
  the same transform.
- **Recipes live where they are used.** A composition repeated in two
  places becomes a plain function returning a `Block` in the toolkit,
  built only from public primitives.

### When a one-off is justified

Only when no composition can express it, or when the composition costs
measurably too much (show the numbers). Record it as a decision in
`docs/decisions.md` with the reason and the primitive it should become
later.

### Review checklist

- Can this be written with existing tags and elements?
- Does any new field name an effect rather than a capability?
- Does the new primitive work on Glim content, Texture and native layers,
  or state where it cannot?
- Do paint and hit testing agree, by sharing one transform?
- Is it gated by a test that fails without it?
