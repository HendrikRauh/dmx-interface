import { render } from "preact";

import bannerLight from "./assets/banner/latte.svg";
import bannerDark from "./assets/banner/mocha.svg";
import { DmxForm } from "./components/dmx-form/DmxForm";

import "./style.scss";

export function App() {
  return (
    <>
      <main>
        <header>
          <picture>
            <source srcset={bannerDark} media="(prefers-color-scheme: dark)" />
            <img src={bannerLight} height="48" alt="ChaosDMX" style={{ maxWidth: "100%" }} />
          </picture>
        </header>
        <DmxForm></DmxForm>
      </main>
    </>
  );
}

render(<App />, document.getElementById("app") || document.body);
