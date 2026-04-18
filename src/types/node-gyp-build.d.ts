declare module "node-gyp-build" {
  export default function load<Addon>(dir: string): Addon;
}
