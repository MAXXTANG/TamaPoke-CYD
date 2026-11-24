// Use the REST artifact ID directly when the artifact SDK omits an upload
// that is already visible in the official REST API.
module.exports = async function deployPages({ github, context, core, artifactId,
  pause = ms => new Promise(resolve => setTimeout(resolve, ms)), now = Date.now }) {
  if (!Number.isSafeInteger(artifactId) || artifactId <= 0) throw new Error("Missing Pages artifact ID");
  const repo = context.repo;
  let artifact;
  for (let attempt = 0; attempt < 10; attempt++) {
    try {
      artifact = (await github.rest.actions.getArtifact({ ...repo, artifact_id: artifactId })).data;
      break;
    } catch (error) {
      if (error.status !== 404 || attempt === 9) throw error;
      await pause(3000);
    }
  }
  if (!artifact || artifact.name !== "github-pages" || artifact.expired ||
      artifact.workflow_run?.id !== context.runId || artifact.workflow_run?.head_sha !== context.sha) {
    throw new Error("Pages artifact does not belong to this workflow run and commit");
  }
  const idToken = await core.getIDToken();
  core.setSecret(idToken);
  const { data: deployment } = await github.request("POST /repos/{owner}/{repo}/pages/deployments", {
    ...repo, artifact_id: artifactId, pages_build_version: context.sha, oidc_token: idToken,
  });
  const deploymentId = deployment.id || deployment.status_url?.split("/").pop();
  if (!deploymentId) throw new Error("Pages did not return a deployment ID");
  core.info("Created Pages deployment " + deploymentId);
  const deadline = now() + 300000;
  while (now() < deadline) {
    await pause(5000);
    let status;
    try {
      status = (await github.request("GET /repos/{owner}/{repo}/pages/deployments/{deployment_id}", {
        ...repo, deployment_id: deploymentId,
      })).data.status;
    } catch (error) {
      if (error.status === 404 || error.status >= 500) continue;
      throw error;
    }
    if (status === "succeed") {
      const pageUrl = deployment.page_url || (await github.rest.repos.getPages(repo)).data.html_url;
      core.setOutput("page_url", pageUrl);core.info("Pages deployment succeeded");return;
    }
    if (["deployment_failed", "deployment_perms_error", "deployment_content_failed", "deployment_cancelled", "deployment_lost"].includes(status)) {
      throw new Error("Pages deployment ended with " + status);
    }
    core.info("Pages deployment status: " + status);
  }
  try { await github.request("POST /repos/{owner}/{repo}/pages/deployments/{deployment_id}/cancel", {
    ...repo, deployment_id: deploymentId,
  }); } catch { /* The deployment may have just finished. */ }
  throw new Error("Timed out waiting for Pages deployment");
};
