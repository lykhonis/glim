// @ts-check
/** @param {string[]} backend archives of the platform's GPU and shell */
export function stackArchives(backend) {
  return ["glim-core", "glim-image", "glim-paint", "glim-paint-gpu", "glim-toolkit", "glim-toolkit-host", "glim-shell", ...backend];
}
