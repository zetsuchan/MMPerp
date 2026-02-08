# MMPerp ImGui Web Terminal

Immediate-mode trading terminal for MMPerp, built with `@mori2003/jsimgui` and Vite.

## Run locally

```bash
cd apps/webui
npm install
npm run dev
```

Open `http://localhost:3000`.

## Oxc suite

This frontend is wired to the full Oxc toolchain:

- `oxlint` for linting
- `oxfmt` for formatting
- `oxc-parser` for AST parsing
- `oxc-transform` for code transforms
- `oxc-minify` for minification checks
- `oxc-resolver` for module resolution checks

Commands:

```bash
npm run lint
npm run format
npm run oxc:suite
npm run build
npm run check
```
