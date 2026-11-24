import test from "node:test";
import assert from "node:assert/strict";
import deploy from "../tools/deploy_pages.cjs";

function fixture({ wrongRun = false, wrongCommit = false, failed = false } = {}) {
  const requests = [], logs = [], secrets = [], outputs = {};
  const context = { repo: { owner: "owner", repo: "project" }, runId: 12, sha: "abcdef" };
  const github = { rest: { actions: { async getArtifact() {
    return { data: { name: "github-pages", expired: false, workflow_run: { id: wrongRun ? 13 : 12, head_sha: wrongCommit ? "different" : "abcdef" } } };
  } } }, async request(route, data) {
    requests.push({ route, data });
    return route.startsWith("POST") ? { data: { id: 7, page_url: "https://owner.github.io/project/" } } : { data: { status: failed ? "deployment_failed" : "succeed" } };
  } };
  const core = { getIDToken: async () => "sample-token", setSecret: value => secrets.push(value), info: value => logs.push(value), setOutput: (key,value) => outputs[key] = value };
  return { github, context, core, artifactId: 99, pause: async () => {}, requests, logs, secrets, outputs };
}
test("Pages publishes the exact current-run artifact and masks its OIDC token", async () => {
  const f = fixture(); await deploy(f);
  assert.equal(f.requests[0].data.artifact_id, 99);assert.equal(f.requests[0].data.pages_build_version, "abcdef");
  assert.deepEqual(f.secrets, ["sample-token"]);assert(!f.logs.join(" ").includes("sample-token"));
  assert.equal(f.outputs.page_url, "https://owner.github.io/project/");
});
test("Pages never publishes an artifact from another run or commit", async () => {
  for (const opts of [{ wrongRun: true }, { wrongCommit: true }]) {
    const f = fixture(opts);await assert.rejects(deploy(f), /does not belong/);assert.equal(f.requests.length, 0);assert.equal(f.secrets.length, 0);
  }
});
test("Pages reports a failed deployment without claiming success", async () => {
  const f = fixture({ failed: true });await assert.rejects(deploy(f), /deployment_failed/);assert.equal(f.outputs.page_url, undefined);
});
