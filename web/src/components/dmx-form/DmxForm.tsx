import { Fragment } from "preact/jsx-runtime";

import { Dropdown } from "../dropdown/Dropdown";
import { Fieldset } from "../fieldset/Fieldset";
import { LabeledInput } from "../labeled-input/LabeledInput";
import { Slider } from "../slider/Slider";

export function DmxForm() {
  return (
    <form>
      <Fieldset legend="Connectivity">
        <LabeledInput label="Mode">
          <Dropdown
            name="mode"
            options={[
              { label: "Access Point", value: "ap" },
              { label: "Station (not yet implemented)", value: "station", disabled: true },
              { label: "Ethernet (not yet implemented)", value: "ethernet", disabled: true },
            ]}
          />
        </LabeledInput>

        <LabeledInput label="WiFi-SSID">
          <input type="text" name="ssid" />
        </LabeledInput>

        <LabeledInput label="Password">
          <input type="password" name="password" />
        </LabeledInput>
      </Fieldset>

      <Fieldset legend="DMX">
        {[0, 1].map((port) => (
          <Fragment key={port}>
            <LabeledInput label={`DMX-Port ${port}`}>
              <Dropdown
                name={`dmx-${port}`}
                options={[
                  { label: "Disabled", value: "disabled" },
                  { label: "Input", value: "input" },
                  { label: "Output", value: "output" },
                ]}
              />
            </LabeledInput>
            <LabeledInput label={`Art-Net Universe for Port ${port}`}>
              <input type="number" name={`universe-${port}`} />
            </LabeledInput>
          </Fragment>
        ))}
      </Fieldset>

      <Fieldset legend="Button & LED">
        <LabeledInput label="LED Brightness">
          <Slider name="brightness" min={0} max={255} relative={true} />
        </LabeledInput>
      </Fieldset>
    </form>
  );
}
