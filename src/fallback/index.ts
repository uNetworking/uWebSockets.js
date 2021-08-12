import { TemplatedApp } from "../../docs/index";
import JsTemplatedApp from "./templated-app";

export const us_listen_socket_close = (listenSocket: any) => {
  return listenSocket.close();
};

/**
 * Maximum delay allowed until an HTTP connection is terminated due to
 * outstanding request or rejected data (slow loris protection)
 */
const HTTP_IDLE_TIMEOUT_S = 10 as const;

export function App(): TemplatedApp {
  return new JsTemplatedApp();
}

export default {
  App,
  us_listen_socket_close
}
