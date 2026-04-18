declare module "node-gyp-build" {
  function bindings<T = unknown>(dir: string): T;
  export default bindings;
}
