import { render } from "preact";

import faviconLight from "./assets/favicon-latte.svg";
import faviconDark from "./assets/favicon-mocha.svg";
import { DmxForm } from "./components/dmx-form/DmxForm";

import "./style.scss";

export function App() {
  return (
    <>
      <main>
        <header>
          <picture>
            <source srcset={faviconDark} media="(prefers-color-scheme: dark)" />
            <img src={faviconLight} width="64" alt="ChaosDMX Logo" />
          </picture>
          <h1>ChaosDMX</h1>
        </header>
        <DmxForm></DmxForm>
      </main>
    </>
  );
}

render(<App />, document.getElementById("app") || document.body);
