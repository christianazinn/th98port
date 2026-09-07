import { createHash } from "node:crypto";
import { readFile } from "node:fs/promises";

function sha256(bytes) {
  return createHash("sha256").update(bytes).digest();
}

function littleEndian(value, bytes) {
  const result = Buffer.alloc(bytes);
  if (bytes === 2) {
    result.writeUInt16LE(value);
  } else if (bytes === 4) {
    result.writeUInt32LE(value);
  } else if (bytes === 8) {
    result.writeBigUInt64LE(BigInt(value));
  } else {
    throw new RangeError("unsupported integer width");
  }
  return result;
}

function recordManifest(magic, records) {
  const magicBytes = Buffer.from(magic, "ascii");
  if (magicBytes.byteLength !== 8) {
    throw new RangeError("identity magic must contain exactly eight ASCII bytes");
  }
  const sorted = [...records].sort((left, right) => left.name.localeCompare(right.name));
  const chunks = [magicBytes, littleEndian(sorted.length, 4)];
  for (const record of sorted) {
    const name = Buffer.from(record.name, "utf8");
    if (name.byteLength === 0 || name.byteLength > 0xffff) {
      throw new RangeError("identity record name length is invalid");
    }
    chunks.push(
      littleEndian(name.byteLength, 2),
      name,
      littleEndian(record.bytes.byteLength, 8),
      sha256(record.bytes),
    );
  }
  return createHash("sha256").update(Buffer.concat(chunks)).digest("hex").toUpperCase();
}

export async function computeRuntimeIdentities({
  verifierPath,
  enedatPath,
  adapterPath,
  identityPath,
  rulesetPath,
  runtimeContractPath,
}) {
  const [verifier, enedat, adapter, identity, ruleset, runtimeContract] =
    await Promise.all([
      readFile(verifierPath),
      readFile(enedatPath),
      readFile(adapterPath),
      readFile(identityPath),
      readFile(rulesetPath),
      readFile(runtimeContractPath),
    ]);
  if (enedat.byteLength !== 5072) {
    throw new RangeError("stock ENEDAT.DAT must contain exactly 5,072 bytes");
  }
  if (!ruleset.subarray(0, 8).equals(Buffer.from("T3RUL001", "ascii"))) {
    throw new RangeError("ruleset contract magic is invalid");
  }
  if (!runtimeContract.subarray(0, 8).equals(Buffer.from("T3RUN001", "ascii"))) {
    throw new RangeError("runtime contract magic is invalid");
  }
  return Object.freeze({
    contentId: recordManifest("T3CNT001", [
      { name: "ENEDAT.DAT", bytes: enedat },
    ]),
    rulesetId: createHash("sha256").update(ruleset).digest("hex").toUpperCase(),
    verifierId: recordManifest("T3VER001", [
      { name: "identity.mjs", bytes: identity },
      { name: "ruleset-v1.txt", bytes: ruleset },
      { name: "runtime-v1.txt", bytes: runtimeContract },
      { name: "server.mjs", bytes: adapter },
      { name: "th03rv-linux-amd64", bytes: verifier },
    ]),
  });
}
