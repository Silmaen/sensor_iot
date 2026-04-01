Align all markdown tables in the project so that:
- All columns are perfectly aligned (content padded with spaces)
- Separator rows use full-width dashes with no spaces: `|---|` not `| --- |`
- Alignment markers (`:---:`, `:---`, `---:`) are preserved

Process all `.md` files in the project root and `docs/` directory (skip `.pio/` and `node_modules/`).

Report which files were modified.
