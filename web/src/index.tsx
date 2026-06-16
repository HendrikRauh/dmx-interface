import { render } from "preact";

import favicon from "./assets/favicon-mocha.svg";
import { DmxForm } from "./components/dmx-form/DmxForm";

import "./style.scss";

export function App() {
  return (
    <>
      <main>
        <header>
          <img src={favicon} width="64" alt="ChaosDMX Logo" />
          <h1>ChaosDMX</h1>
        </header>
        <DmxForm></DmxForm>
      </main>
    </>
  );
}

render(<App />, document.getElementById("app") || document.body);
