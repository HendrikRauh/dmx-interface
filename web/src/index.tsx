import { render } from "preact";

import { DmxForm } from "./components/dmx-form/DmxForm";

import "./style.scss";

export function App() {
  return (
    <main>
      <DmxForm></DmxForm>
    </main>
  );
}

render(<App />, document.getElementById("app") || document.body);
