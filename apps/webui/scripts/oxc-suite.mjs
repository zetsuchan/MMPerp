import { readFileSync } from 'node:fs';
import { dirname, relative, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';

import { minifySync } from 'oxc-minify';
import { parseSync } from 'oxc-parser';
import { ResolverFactory } from 'oxc-resolver';
import { transformSync } from 'oxc-transform';

const formatErrors = (phase, errors) => {
  const items = errors.map((error) => {
    const frame = error.codeframe ? `\n${error.codeframe}` : '';
    return `[${phase}] ${error.message}${frame}`;
  });
  return items.join('\n\n');
};

const thisFile = fileURLToPath(import.meta.url);
const projectRoot = resolve(dirname(thisFile), '..');
const entryFile = resolve(projectRoot, 'src/main.ts');
const source = readFileSync(entryFile, 'utf8');

const parsed = parseSync(entryFile, source, {
  lang: 'ts',
  sourceType: 'module',
  showSemanticErrors: true,
});
if (parsed.errors.length > 0) {
  throw new Error(formatErrors('parser', parsed.errors));
}

const transformed = transformSync(entryFile, source, {
  lang: 'ts',
  sourceType: 'module',
  target: 'es2022',
  jsx: 'preserve',
});
if (transformed.errors.length > 0) {
  throw new Error(formatErrors('transform', transformed.errors));
}

const minified = minifySync('main.js', transformed.code, {
  module: true,
  compress: {
    target: 'es2022',
    dropDebugger: true,
  },
  mangle: false,
  codegen: {
    removeWhitespace: true,
  },
});
if (minified.errors.length > 0) {
  throw new Error(formatErrors('minify', minified.errors));
}

const resolver = new ResolverFactory({
  tsconfig: 'auto',
  conditionNames: ['node', 'import'],
  extensions: ['.ts', '.tsx', '.js', '.mjs', '.json'],
});
const imgui = resolver.resolveFileSync(entryFile, '@mori2003/jsimgui');
if (!imgui.path || imgui.error) {
  throw new Error(
    `[resolver] failed to resolve @mori2003/jsimgui: ${imgui.error ?? 'unknown error'}`,
  );
}
const self = resolver.resolveFileSync(entryFile, './main.ts');
if (!self.path || self.error) {
  throw new Error(`[resolver] failed to resolve ./main.ts: ${self.error ?? 'unknown error'}`);
}

const report = {
  entry: relative(projectRoot, entryFile),
  astRootNodes: parsed.program.body.length,
  staticImports: parsed.module.staticImports.length,
  comments: parsed.comments.length,
  transformedBytes: Buffer.byteLength(transformed.code, 'utf8'),
  minifiedBytes: Buffer.byteLength(minified.code, 'utf8'),
  resolution: {
    imgui: relative(projectRoot, imgui.path),
    self: relative(projectRoot, self.path),
  },
};

console.log('Oxc suite check passed');
console.log(JSON.stringify(report, null, 2));
