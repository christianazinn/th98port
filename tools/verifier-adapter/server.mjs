import { createHash } from "node:crypto";
import { execFile } from "node:child_process";
import { access, mkdtemp, rm, writeFile } from "node:fs/promises";
import { constants as fsConstants } from "node:fs";
import { createServer } from "node:http";
import { tmpdir } from "node:os";
import { isAbsolute, join } from "node:path";
import { pathToFileURL } from "node:url";

import { computeRuntimeIdentities } from "./identity.mjs";

const protocol = "T3RV-SERVICE-1";
const replayContentType = "application/vnd.th03.v14-replay";
const identityPattern = /^[0-9A-F]{64}$/;
const rejectedReasons = new Set([
  "invalid_replay",
  "unsupported_replay",
  "split_mismatch",
]);

export const limits = Object.freeze({
  replayBytes: 1024 * 1024,
  stdoutBytes: 16 * 1024,
  stderrBytes: 8 * 1024,
  wallTimeMs: 10_000,
});

const errorEnvelope = Object.freeze({ protocol, outcome: "error" });

function jsonResponse(response, status = 200) {
  return {
    status,
    headers: {
      "cache-control": "no-store",
      "content-type": "application/json; charset=utf-8",
    },
    body: JSON.stringify(response),
  };
}

function discardRequestBody(request) {
  request.on("error", () => {});
  request.resume();
}

function sha256(bytes) {
  return createHash("sha256").update(bytes).digest("hex").toUpperCase();
}

function plainObject(value) {
  return typeof value === "object" && value !== null && !Array.isArray(value);
}

function parseVerifierOutput(result, replayId, identities) {
  if (
    result.timedOut || result.overflow ||
    !Buffer.isBuffer(result.stdout) || !Buffer.isBuffer(result.stderr) ||
    result.stdout.byteLength > limits.stdoutBytes ||
    result.stderr.byteLength > limits.stderrBytes
  ) {
    return errorEnvelope;
  }

  let text;
  try {
    text = new TextDecoder("utf-8", { fatal: true }).decode(result.stdout);
  } catch {
    return errorEnvelope;
  }
  const line = text.endsWith("\r\n") ? text.slice(0, -2) :
    (text.endsWith("\n") ? text.slice(0, -1) : undefined);
  if (line === undefined || line.includes("\n") || line.includes("\r")) {
    return errorEnvelope;
  }

  let parsed;
  try {
    parsed = JSON.parse(line);
  } catch {
    return errorEnvelope;
  }
  if (!plainObject(parsed) || parsed.protocol !== protocol) {
    return errorEnvelope;
  }

  if (parsed.outcome === "accepted") {
    if (
      result.exitCode !== 0 ||
      Object.hasOwn(parsed, "replayId") || Object.hasOwn(parsed, "contentId") ||
      Object.hasOwn(parsed, "rulesetId") || Object.hasOwn(parsed, "verifierId")
    ) {
      return errorEnvelope;
    }
    return {
      ...parsed,
      replayId,
      contentId: identities.contentId,
      rulesetId: identities.rulesetId,
      verifierId: identities.verifierId,
    };
  }

  if (
    parsed.outcome === "rejected" &&
    (result.exitCode === 1 || result.exitCode === 2) &&
    typeof parsed.reason === "string" && rejectedReasons.has(parsed.reason)
  ) {
    return { protocol, outcome: "rejected", reason: parsed.reason };
  }
  return errorEnvelope;
}

export function executeVerifierProcess({
  verifierPath,
  enedatPath,
  replayPath,
  timeoutMs = limits.wallTimeMs,
  prefixArguments = [],
}) {
  return new Promise((resolve) => {
    execFile(
      verifierPath,
      [...prefixArguments, replayPath, "--enedat", enedatPath, "--service-json"],
      {
        encoding: "buffer",
        env: {
          HOME: "/nonexistent",
          LANG: "C",
          LC_ALL: "C",
          PATH: process.env.PATH ?? "",
          SYSTEMROOT: process.env.SYSTEMROOT ?? "",
          TMPDIR: tmpdir(),
        },
        killSignal: "SIGKILL",
        maxBuffer: limits.stdoutBytes + 1,
        timeout: timeoutMs,
        windowsHide: true,
      },
      (error, stdout, stderr) => {
        const signal = error && typeof error === "object" ? error.signal : undefined;
        const code = error && typeof error === "object" ? error.code : undefined;
        resolve({
          exitCode: error === null ? 0 : (Number.isInteger(code) ? code : null),
          stdout: Buffer.isBuffer(stdout) ? stdout : Buffer.from(stdout ?? ""),
          stderr: Buffer.isBuffer(stderr) ? stderr : Buffer.from(stderr ?? ""),
          timedOut: signal === "SIGKILL" && code === null,
          overflow: error !== null && typeof error === "object" &&
            String(error.message ?? "").includes("maxBuffer"),
        });
      },
    );
  });
}

export async function verifyReplay(bytes, replayId, configuration, execute = executeVerifierProcess) {
  if (
    !Buffer.isBuffer(bytes) || bytes.byteLength === 0 ||
    bytes.byteLength > limits.replayBytes || !identityPattern.test(replayId) ||
    sha256(bytes) !== replayId
  ) {
    return errorEnvelope;
  }

  const directory = await mkdtemp(join(tmpdir(), "th03-verifier-"));
  const replayPath = join(directory, "submission.rpy");
  try {
    await writeFile(replayPath, bytes, { flag: "wx", mode: 0o600 });
    const result = await execute({
      verifierPath: configuration.verifierPath,
      enedatPath: configuration.enedatPath,
      replayPath,
      timeoutMs: configuration.timeoutMs,
      prefixArguments: configuration.prefixArguments ?? [],
    });
    return parseVerifierOutput(result, replayId, configuration);
  } catch {
    return errorEnvelope;
  } finally {
    await rm(directory, { recursive: true, force: true });
  }
}

export async function readBoundedReplay(request) {
  const declared = request.headers["content-length"];
  if (declared !== undefined) {
    const parsed = Number(declared);
    if (!Number.isSafeInteger(parsed) || parsed < 1 || parsed > limits.replayBytes) {
      throw new RangeError("replay body length is outside the accepted range");
    }
  }
  const chunks = [];
  let length = 0;
  for await (const chunk of request) {
    const bytes = Buffer.isBuffer(chunk) ? chunk : Buffer.from(chunk);
    length += bytes.byteLength;
    if (length > limits.replayBytes) {
      throw new RangeError("replay body exceeds the accepted range");
    }
    chunks.push(bytes);
  }
  if (length === 0) {
    throw new RangeError("replay body is empty");
  }
  return Buffer.concat(chunks, length);
}

export function loadConfiguration(environment = process.env) {
  const configuration = {
    verifierPath: environment.TH03_VERIFIER_PATH ?? "/opt/th03/th03rv",
    enedatPath: environment.TH03_ENEDAT_PATH ?? "/opt/th03/ENEDAT.DAT",
    adapterPath: environment.TH03_ADAPTER_PATH ?? "/opt/th03/server.mjs",
    identityPath: environment.TH03_IDENTITY_PATH ?? "/opt/th03/identity.mjs",
    rulesetPath: environment.TH03_RULESET_PATH ?? "/opt/th03/ruleset-v1.txt",
    runtimeContractPath: environment.TH03_RUNTIME_CONTRACT_PATH ??
      "/opt/th03/runtime-v1.txt",
    timeoutMs: Number(environment.TH03_VERIFIER_TIMEOUT_MS ?? limits.wallTimeMs),
  };
  if (
    !isAbsolute(configuration.verifierPath) || !isAbsolute(configuration.enedatPath) ||
    !isAbsolute(configuration.adapterPath) || !isAbsolute(configuration.identityPath) ||
    !isAbsolute(configuration.rulesetPath) ||
    !isAbsolute(configuration.runtimeContractPath) ||
    !Number.isInteger(configuration.timeoutMs) || configuration.timeoutMs < 1 ||
    configuration.timeoutMs > 12_000
  ) {
    throw new Error("verifier configuration is incomplete or invalid");
  }
  return configuration;
}

export function createVerifierServer(configuration, execute = executeVerifierProcess) {
  let verifierBusy = false;
  return createServer(async (request, response) => {
    const url = new URL(request.url ?? "/", "http://verifier.invalid");
    if (url.pathname === "/healthz" && request.method === "GET") {
      response.writeHead(200, { "cache-control": "no-store", "content-type": "text/plain" });
      response.end("ok\n");
      return;
    }
    if (url.pathname === "/v1/identity" && request.method === "GET") {
      const result = jsonResponse({
        protocol: "T3RV-IDENTITY-1",
        contentId: configuration.contentId,
        rulesetId: configuration.rulesetId,
        verifierId: configuration.verifierId,
      });
      response.writeHead(result.status, result.headers);
      response.end(result.body);
      return;
    }
    if (url.pathname !== "/v1/verify") {
      discardRequestBody(request);
      response.writeHead(404, { "cache-control": "no-store" });
      response.end();
      return;
    }
    if (request.method !== "POST") {
      discardRequestBody(request);
      response.writeHead(405, { "allow": "POST", "cache-control": "no-store" });
      response.end();
      return;
    }
    if (request.headers["content-type"] !== replayContentType) {
      discardRequestBody(request);
      const result = jsonResponse(errorEnvelope, 415);
      response.writeHead(result.status, result.headers);
      response.end(result.body);
      return;
    }
    const replayId = request.headers["x-th03-replay-sha256"];
    if (typeof replayId !== "string" || !identityPattern.test(replayId)) {
      discardRequestBody(request);
      const result = jsonResponse(errorEnvelope, 400);
      response.writeHead(result.status, result.headers);
      response.end(result.body);
      return;
    }
    if (verifierBusy) {
      discardRequestBody(request);
      const result = jsonResponse(errorEnvelope, 503);
      response.writeHead(result.status, result.headers);
      response.end(result.body);
      return;
    }

    verifierBusy = true;
    try {
      const bytes = await readBoundedReplay(request);
      const result = jsonResponse(await verifyReplay(bytes, replayId, configuration, execute));
      response.writeHead(result.status, result.headers);
      response.end(result.body);
    } catch (error) {
      const status = error instanceof RangeError ? 413 : 500;
      const result = jsonResponse(errorEnvelope, status);
      response.writeHead(result.status, result.headers);
      response.end(result.body);
    } finally {
      verifierBusy = false;
    }
  });
}

export async function validateRuntimeFiles(configuration) {
  await Promise.all([
    access(configuration.verifierPath, fsConstants.R_OK | fsConstants.X_OK),
    access(configuration.enedatPath, fsConstants.R_OK),
    access(configuration.adapterPath, fsConstants.R_OK),
    access(configuration.identityPath, fsConstants.R_OK),
    access(configuration.rulesetPath, fsConstants.R_OK),
    access(configuration.runtimeContractPath, fsConstants.R_OK),
  ]);
  return await computeRuntimeIdentities(configuration);
}

async function main() {
  const baseConfiguration = loadConfiguration();
  const configuration = {
    ...baseConfiguration,
    ...await validateRuntimeFiles(baseConfiguration),
  };
  const port = Number(process.env.PORT ?? 8080);
  if (!Number.isInteger(port) || port < 1 || port > 65_535) {
    throw new Error("PORT is invalid");
  }
  const server = createVerifierServer(configuration);
  server.listen(port, "0.0.0.0");
}

if (process.argv[1] && import.meta.url === pathToFileURL(process.argv[1]).href) {
  main().catch(() => {
    process.exitCode = 1;
  });
}
