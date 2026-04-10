const fs = require("fs");
const path = require("path");
const sharp = require("sharp");
const { mathjax } = require("mathjax-full/js/mathjax.js");
const { TeX } = require("mathjax-full/js/input/tex.js");
const { SVG } = require("mathjax-full/js/output/svg.js");
const { liteAdaptor } = require("mathjax-full/js/adaptors/liteAdaptor.js");
const { RegisterHTMLHandler } = require("mathjax-full/js/handlers/html.js");

const repoRoot = path.resolve(__dirname, "..");
const dataDir = path.join(repoRoot, "data");
const generatedDir = path.join(repoRoot, "generated_data");
const outDir = path.join(generatedDir, "m");
const adaptor = liteAdaptor();
RegisterHTMLHandler(adaptor);
const tex = new TeX({
  packages: ["base", "ams", "noerrors", "noundefined"],
});
const svg = new SVG({
  fontCache: "none",
});
const html = mathjax.document("", {
  InputJax: tex,
  OutputJax: svg,
});

function ensureDir(dir) {
  fs.mkdirSync(dir, { recursive: true });
}

function slugify(name) {
  return name.toLowerCase().replace(/[^a-z0-9]+/g, "_").replace(/^_+|_+$/g, "");
}

function mathMarker(relPath, width, height) {
  return `[[MATH_PBM|${relPath}|${width}|${height}]]`;
}

async function renderMathToPbm(latex, outPath) {
  const node = html.convert(latex, { display: true });
  const svgMarkup = adaptor.innerHTML(node);

  const image = sharp(Buffer.from(svgMarkup))
    .flatten({ background: "#ffffff" })
    .trim()
    .threshold(192)
    .toColourspace("b-w");

  const { data, info } = await image
    .raw()
    .toBuffer({ resolveWithObject: true });
  const rowBytes = Math.ceil(info.width / 8);
  const bits = Buffer.alloc(rowBytes * info.height, 0x00);

  for (let y = 0; y < info.height; y += 1) {
    for (let x = 0; x < info.width; x += 1) {
      const pixel = data[y * info.width + x];
      if (pixel < 128) {
        const index = y * rowBytes + Math.floor(x / 8);
        bits[index] |= 0x80 >> (x & 7);
      }
    }
  }

  const header = Buffer.from(`P4\n${info.width} ${info.height}\n`, "ascii");
  fs.writeFileSync(outPath, Buffer.concat([header, bits]));
  return { width: info.width, height: info.height };
}

async function processFile(filePath) {
  const source = fs.readFileSync(filePath, "utf8");
  const base = slugify(path.basename(filePath, ".md"));
  let index = 0;
  const transformed = [];
  const lines = source.split(/\r?\n/);
  let inMathBlock = false;
  let mathLines = [];

  for (const line of lines) {
    const trimmed = line.trim();
    if (line.trim() === "$$") {
      if (!inMathBlock) {
        inMathBlock = true;
        mathLines = [];
      } else {
        const expr = mathLines.join("\n").trim();
        const fileName = `${base}_${String(index).padStart(3, "0")}.pbm`;
        const relPath = `m/${fileName}`;
        const outPath = path.join(outDir, fileName);
        const image = await renderMathToPbm(expr, outPath);
        transformed.push(mathMarker(relPath, image.width, image.height));
        index += 1;
        inMathBlock = false;
        mathLines = [];
      }
      continue;
    }

    if (inMathBlock) {
      mathLines.push(line);
    } else {
      const inlineMatches = [...line.matchAll(/\$([^$\n]+)\$/g)];
      if (inlineMatches.length > 0) {
        let rebuilt = "";
        let cursor = 0;
        for (const match of inlineMatches) {
          rebuilt += line.slice(cursor, match.index);
          const expr = match[1].trim();
          const fileName = `${base}_${String(index).padStart(3, "0")}.pbm`;
          const relPath = `m/${fileName}`;
          const outPath = path.join(outDir, fileName);
          const image = await renderMathToPbm(expr, outPath);
          rebuilt += mathMarker(relPath, image.width, image.height);
          index += 1;
          cursor = match.index + match[0].length;
        }
        rebuilt += line.slice(cursor);
        transformed.push(rebuilt);
        continue;
      }
      transformed.push(line);
    }
  }

  const outPath = path.join(generatedDir, path.basename(filePath));
  fs.writeFileSync(outPath, transformed.join("\n"), "utf8");
}

async function main() {
  ensureDir(generatedDir);
  ensureDir(outDir);
  for (const name of fs.readdirSync(generatedDir)) {
    fs.rmSync(path.join(generatedDir, name), { recursive: true, force: true });
  }
  ensureDir(outDir);

  const files = fs.readdirSync(dataDir).map((name) => path.join(dataDir, name));

  for (const file of files) {
    const stat = fs.statSync(file);
    if (stat.isDirectory()) {
      continue;
    }
    if (file.endsWith(".md")) {
      await processFile(file);
    } else {
      fs.copyFileSync(file, path.join(generatedDir, path.basename(file)));
    }
  }
}

main().catch((error) => {
  console.error(error);
  process.exit(1);
});
