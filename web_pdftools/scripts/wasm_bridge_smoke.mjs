#!/usr/bin/env node

import fs from 'node:fs/promises';
import path from 'node:path';

import { createPdftoolsWasmBackend } from '../wasm/pdftools_wasm_backend.mjs';

function parseArgs(argv) {
  const result = {
    op: 'merge',
    inputs: [],
    output: '',
    dumpIr: ''
  };

  for (let i = 0; i < argv.length; i += 1) {
    const arg = argv[i];
    if (arg === '--op') {
      result.op = argv[i + 1] || '';
      i += 1;
      continue;
    }
    if (arg === '--input') {
      result.inputs.push(argv[i + 1] || '');
      i += 1;
      continue;
    }
    if (arg === '--output') {
      result.output = argv[i + 1] || '';
      i += 1;
      continue;
    }
    if (arg === '--dump-ir') {
      result.dumpIr = argv[i + 1] || '';
      i += 1;
      continue;
    }
  }

  return result;
}

function usage() {
  console.log('Usage:');
  console.log('  node scripts/wasm_bridge_smoke.mjs --op merge --input a.pdf --input b.pdf --output out.pdf');
  console.log('  node scripts/wasm_bridge_smoke.mjs --op pdf2docx --input a.pdf --output out.docx [--dump-ir ir.json]');
}

function normalizeExt(filePath, fallback) {
  const ext = path.extname(filePath || '');
  if (!ext) {
    return fallback;
  }
  return ext.toLowerCase();
}

async function readInputs(hostPaths) {
  const payloads = [];
  for (let i = 0; i < hostPaths.length; i += 1) {
    const hostPath = hostPaths[i];
    const data = await fs.readFile(hostPath);
    payloads.push({
      hostPath,
      wasmPath: `/work/in/${i}${normalizeExt(hostPath, '.bin')}`,
      data: new Uint8Array(data)
    });
  }
  return payloads;
}

function buildMergeBridge(inputs, output) {
  const outputWasmPath = `/work/out/0${normalizeExt(output, '.pdf')}`;
  return {
    __hostFsBridge: true,
    task: {
      type: 'merge',
      inputPdfs: inputs.map((item) => item.wasmPath),
      outputPdf: outputWasmPath,
      overwrite: true
    },
    inputFiles: inputs.map((item) => ({
      path: item.wasmPath,
      data: item.data
    })),
    outputFiles: [
      {
        path: outputWasmPath,
        hostPath: output
      }
    ]
  };
}

function buildPdf2DocxBridge(inputs, output, dumpIr) {
  const outputWasmPath = `/work/out/0${normalizeExt(output, '.docx')}`;
  const outputFiles = [
    {
      path: outputWasmPath,
      hostPath: output
    }
  ];

  let dumpIrWasmPath = '';
  if (dumpIr) {
    dumpIrWasmPath = `/work/out/1${normalizeExt(dumpIr, '.json')}`;
    outputFiles.push({
      path: dumpIrWasmPath,
      hostPath: dumpIr
    });
  }

  return {
    __hostFsBridge: true,
    task: {
      type: 'pdf2docx',
      inputPdf: inputs[0].wasmPath,
      outputDocx: outputWasmPath,
      dumpIrPath: dumpIrWasmPath,
      overwrite: true
    },
    inputFiles: [
      {
        path: inputs[0].wasmPath,
        data: inputs[0].data
      }
    ],
    outputFiles
  };
}

async function main() {
  const args = parseArgs(process.argv.slice(2));
  if (!args.op || !args.output || args.inputs.length === 0) {
    usage();
    process.exitCode = 2;
    return;
  }

  if (args.op === 'merge' && args.inputs.length < 2) {
    console.error('merge requires at least two --input');
    process.exitCode = 2;
    return;
  }

  if (args.op === 'pdf2docx' && args.inputs.length !== 1) {
    console.error('pdf2docx requires exactly one --input');
    process.exitCode = 2;
    return;
  }

  const moduleUrl = new URL('../wasm/dist/pdftools_wasm.mjs', import.meta.url).toString();
  const wasmBinaryPath = new URL('../wasm/dist/pdftools_wasm.wasm', import.meta.url);
  const wasmBinary = await fs.readFile(wasmBinaryPath);

  const backend = await createPdftoolsWasmBackend({
    moduleUrl,
    moduleOptions: { wasmBinary }
  });
  await backend.init();

  try {
    const inputs = await readInputs(args.inputs);
    const bridgePayload =
      args.op === 'merge'
        ? buildMergeBridge(inputs, args.output)
        : buildPdf2DocxBridge(inputs, args.output, args.dumpIr);

    const runResult = await backend.run(bridgePayload);
    if (!runResult.bridge || runResult.wasmResult?.ok !== true) {
      throw new Error('WASM bridge operation failed');
    }

    for (const outputFile of runResult.outputFiles || []) {
      if (!outputFile.hostPath || !outputFile.data) {
        continue;
      }
      await fs.mkdir(path.dirname(outputFile.hostPath), { recursive: true });
      await fs.writeFile(outputFile.hostPath, outputFile.data);
    }

    console.log(
      JSON.stringify({
        ok: true,
        op: args.op,
        outputs: (runResult.outputFiles || []).map((item) => ({
          hostPath: item.hostPath,
          bytes: item.data?.length || 0
        }))
      })
    );
  } finally {
    await backend.dispose();
  }
}

main().catch((error) => {
  console.error(error?.stack || String(error));
  process.exitCode = 1;
});
