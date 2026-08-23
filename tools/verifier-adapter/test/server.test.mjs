import assert from "node:assert/strict";
import { createHash } from "node:crypto";
import { once } from "node:events";
import { mkdtemp, rm, writeFile } from "node:fs/promises";
import { request as httpRequest } from "node:http";
import { tmpdir } from "node:os";
import { join } from "node:path";
import test from "node:test";

import {
  createVerifierServer,
  executeVerifierProcess,
  limits,
  loadConfiguration,
  readBoundedReplay,
  verifyReplay,
} from "../server.mjs";
import { computeRuntimeIdentities } from "../identity.mjs";

const identity = (byte) => byte.repeat(64);
const configuration = Object.freeze({
  verifierPath: "/fixed/th03rv",
  enedatPath: "/fixed/ENEDAT.DAT",
  contentId: identity("A"),
  rulesetId: identity("B"),
  verifierId: identity("C"),
  timeoutMs: 100,
});
const replay = Buffer.from("T3RPLY14 deterministic fixture");
const replayId = createHash("sha256").update(replay).digest("hex").toUpperCase();

function nativeResult(value, exitCode = 0, stderr = "") {
  return {
    exitCode,
    stdout: Buffer.from(`${JSON.stringify(value)}\n`),
    stderr: Buffer.from(stderr),
    timedOut: false,
    overflow: false,
  };
}

test("accepted native output is bound to the exact request and configured identities", async () => {
  let invocation;
  const execute = async (value) => {
    invocation = value;
    return nativeResult({
      protocol: "T3RV-SERVICE-1",
      outcome: "accepted",
      sourceVersion: 14,
      finalized: true,
      splitsMatch: true,
      checkpointsMatch: true,
      displayName: "TEST",
      category: { mode: "story", rank: 0, character: 0, startStage: 0, startRound: 0 },
      result: { kind: "clear", score: 1000, endStage: 8, continuesUsed: 0 },
      autofire: 0,
      timedFrames: 1000,
      slowFrames: 0,
      dosDate: 1,
    });
  };
  const result = await verifyReplay(replay, replayId, configuration, execute);
  assert.equal(result.outcome, "accepted");
  assert.equal(result.replayId, replayId);
  assert.equal(result.contentId, configuration.contentId);
  assert.equal(result.rulesetId, configuration.rulesetId);
  assert.equal(result.verifierId, configuration.verifierId);
  assert.equal(invocation.verifierPath, configuration.verifierPath);
  assert.equal(invocation.enedatPath, configuration.enedatPath);
  assert.deepEqual(invocation.prefixArguments, []);
});

test("native rejection is forwarded only for the fixed reason and exit contract", async () => {
  const accepted = await verifyReplay(replay, replayId, configuration, async () =>
    nativeResult({ protocol: "T3RV-SERVICE-1", outcome: "rejected", reason: "invalid_replay" }, 2));
  assert.deepEqual(accepted, {
    protocol: "T3RV-SERVICE-1",
    outcome: "rejected",
    reason: "invalid_replay",
  });
  const inconsistent = await verifyReplay(replay, replayId, configuration, async () =>
    nativeResult({ protocol: "T3RV-SERVICE-1", outcome: "rejected", reason: "invalid_replay" }, 0));
  assert.equal(inconsistent.outcome, "error");

  const windowsLineEnding = await verifyReplay(replay, replayId, configuration, async () => ({
    ...nativeResult({}, 2),
    stdout: Buffer.from('{"protocol":"T3RV-SERVICE-1","outcome":"rejected","reason":"invalid_replay"}\r\n'),
  }));
  assert.equal(windowsLineEnding.outcome, "rejected");
});

test("digest mismatch, extra stdout, timeout, overflow, and stderr overflow fail closed", async () => {
  let calls = 0;
  const execute = async () => {
    calls++;
    return nativeResult({ protocol: "T3RV-SERVICE-1", outcome: "error" }, 3);
  };
  assert.equal((await verifyReplay(replay, identity("D"), configuration, execute)).outcome, "error");
  assert.equal(calls, 0);

  const extra = await verifyReplay(replay, replayId, configuration, async () => ({
    ...nativeResult({ protocol: "T3RV-SERVICE-1", outcome: "error" }, 3),
    stdout: Buffer.from('{"protocol":"T3RV-SERVICE-1","outcome":"error"}\nextra\n'),
  }));
  assert.equal(extra.outcome, "error");
  const timed = await verifyReplay(replay, replayId, configuration, async () => ({
    ...nativeResult({}, 0), timedOut: true,
  }));
  assert.equal(timed.outcome, "error");
  const overflow = await verifyReplay(replay, replayId, configuration, async () => ({
    ...nativeResult({}, 0), overflow: true,
  }));
  assert.equal(overflow.outcome, "error");
  const stderr = await verifyReplay(replay, replayId, configuration, async () =>
    nativeResult({ protocol: "T3RV-SERVICE-1", outcome: "error" }, 3, "x".repeat(limits.stderrBytes + 1)));
  assert.equal(stderr.outcome, "error");
});

test("configuration is all-or-nothing and bounds the child deadline", () => {
  assert.deepEqual(loadConfiguration({
    TH03_VERIFIER_PATH: "/opt/th03/th03rv",
    TH03_ENEDAT_PATH: "/opt/th03/ENEDAT.DAT",
    TH03_VERIFIER_TIMEOUT_MS: "12000",
  }).timeoutMs, 12_000);
  assert.throws(() => loadConfiguration({
    TH03_VERIFIER_PATH: "relative/th03rv",
  }));
  assert.throws(() => loadConfiguration({
    TH03_VERIFIER_TIMEOUT_MS: "12001",
  }));
});

test("runtime identities bind exact content, rules, verifier, adapter, and runtime policy", async (context) => {
  const directory = await mkdtemp(join(tmpdir(), "th03-verifier-identity-test-"));
  context.after(() => rm(directory, { recursive: true, force: true }));
  const paths = {
    verifierPath: join(directory, "th03rv"),
    enedatPath: join(directory, "ENEDAT.DAT"),
    adapterPath: join(directory, "server.mjs"),
    identityPath: join(directory, "identity.mjs"),
    rulesetPath: join(directory, "ruleset-v1.txt"),
    runtimeContractPath: join(directory, "runtime-v1.txt"),
  };
  await Promise.all([
    writeFile(paths.verifierPath, "verifier-a"),
    writeFile(paths.enedatPath, Buffer.alloc(5072, 1)),
    writeFile(paths.adapterPath, "adapter-a"),
    writeFile(paths.identityPath, "identity-a"),
    writeFile(paths.rulesetPath, "T3RUL001\nrules-a\n"),
    writeFile(paths.runtimeContractPath, "T3RUN001\nruntime-a\n"),
  ]);
  const first = await computeRuntimeIdentities(paths);
  assert.match(first.contentId, /^[0-9A-F]{64}$/);
  assert.match(first.rulesetId, /^[0-9A-F]{64}$/);
  assert.match(first.verifierId, /^[0-9A-F]{64}$/);
  assert.deepEqual(await computeRuntimeIdentities(paths), first);

  await writeFile(paths.verifierPath, "verifier-b");
  const changedVerifier = await computeRuntimeIdentities(paths);
  assert.equal(changedVerifier.contentId, first.contentId);
  assert.equal(changedVerifier.rulesetId, first.rulesetId);
  assert.notEqual(changedVerifier.verifierId, first.verifierId);

  await writeFile(paths.enedatPath, Buffer.alloc(5072, 2));
  const changedContent = await computeRuntimeIdentities(paths);
  assert.notEqual(changedContent.contentId, first.contentId);
  assert.equal(changedContent.rulesetId, first.rulesetId);
});

test("the real child-process owner kills a verifier that exceeds its deadline", async (context) => {
  const directory = await mkdtemp(join(tmpdir(), "th03-verifier-child-test-"));
  context.after(() => rm(directory, { recursive: true, force: true }));
  const script = join(directory, "stall.mjs");
  await writeFile(script, "setInterval(() => {}, 1000);\n", { mode: 0o600 });
  const result = await executeVerifierProcess({
    verifierPath: process.execPath,
    enedatPath: join(directory, "ENEDAT.DAT"),
    replayPath: join(directory, "replay.rpy"),
    timeoutMs: 50,
    prefixArguments: [script],
  });
  assert.equal(result.timedOut, true);
  assert.equal(result.exitCode, null);
});

function send(port, options = {}) {
  const {
    method = "POST",
    path = "/v1/verify",
    headers: sourceHeaders = {},
  } = options;
  const headers = { ...sourceHeaders };
  const body = Object.hasOwn(options, "body") ? options.body : replay;
  return new Promise((resolve, reject) => {
    const request = httpRequest({ host: "127.0.0.1", port, method, path, headers }, (response) => {
      const chunks = [];
      response.on("data", (chunk) => chunks.push(chunk));
      response.on("end", () => resolve({
        status: response.statusCode,
        body: Buffer.concat(chunks).toString("utf8"),
      }));
    });
    request.on("error", reject);
    if (body !== undefined && body !== null) {
      request.end(body);
    } else {
      request.end();
    }
  });
}

test("HTTP boundary accepts only the fixed route, method, type, digest, and bounded body", async (context) => {
  const execute = async () => nativeResult({
    protocol: "T3RV-SERVICE-1",
    outcome: "rejected",
    reason: "unsupported_replay",
  }, 1);
  const server = createVerifierServer(configuration, execute).listen(0, "127.0.0.1");
  await once(server, "listening");
  context.after(() => {
    server.closeAllConnections();
    server.close();
  });
  const port = server.address().port;
  const headers = {
    "content-length": replay.byteLength,
    "content-type": "application/vnd.th03.v14-replay",
    "x-th03-replay-sha256": replayId,
  };
  assert.equal((await send(port, { headers })).status, 200);
  const identityResponse = await send(port, {
    method: "GET", path: "/v1/identity", body: undefined,
  });
  assert.equal(identityResponse.status, 200);
  assert.equal(JSON.parse(identityResponse.body).verifierId, configuration.verifierId);
  assert.equal((await send(port, { path: "/wrong", headers })).status, 404);
  assert.equal((await send(port, { method: "GET", body: undefined })).status, 405);
  assert.equal((await send(port, { headers: { ...headers, "content-type": "application/octet-stream" } })).status, 415);
  assert.equal((await send(port, { headers: { ...headers, "x-th03-replay-sha256": identity("D") } })).status, 200);
  await assert.rejects(readBoundedReplay({
    headers: { "content-length": String(limits.replayBytes + 1) },
    async *[Symbol.asyncIterator]() {},
  }), RangeError);
});

test("one lite Container admits only one native verifier process at a time", async (context) => {
  let release;
  const blocked = new Promise((resolve) => {
    release = resolve;
  });
  let started;
  const entered = new Promise((resolve) => {
    started = resolve;
  });
  const execute = async () => {
    started();
    await blocked;
    return nativeResult({
      protocol: "T3RV-SERVICE-1",
      outcome: "rejected",
      reason: "unsupported_replay",
    }, 1);
  };
  const server = createVerifierServer(configuration, execute).listen(0, "127.0.0.1");
  await once(server, "listening");
  context.after(() => {
    server.closeAllConnections();
    server.close();
  });
  const port = server.address().port;
  const headers = {
    "content-length": replay.byteLength,
    "content-type": "application/vnd.th03.v14-replay",
    "x-th03-replay-sha256": replayId,
  };
  const first = send(port, { headers });
  await entered;
  assert.equal((await send(port, { headers })).status, 503);
  release();
  assert.equal((await first).status, 200);
});
