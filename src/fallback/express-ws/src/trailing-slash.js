module.exports = function addTrailingSlash(string) {
  let suffixed = string;
  if (suffixed.charAt(suffixed.length - 1) !== '/') {
    suffixed = `${suffixed}/`;
  }
  return suffixed;
}
