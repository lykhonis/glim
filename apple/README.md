# Using Glim from Xcode

`pnpm xcframework` builds `apple/Glim.xcframework` (macOS, iOS and tvOS, with simulator slices) holding the Metal toolkit stack as one static library and its headers. Add `apple/` as a local Swift package and link the `Glim` product; it brings the system frameworks with it. `apple/Sample` is a working Xcode app; `pnpm apple-sample` builds it for every slice.
