import { useEffect, useState } from "preact/hooks";
import { Fragment } from "preact/jsx-runtime";

import { deepMerged, DeepPartial } from "../../util/deep-merge";
import { Dropdown } from "../dropdown/Dropdown";
import { Fieldset } from "../fieldset/Fieldset";
import { LabeledInput } from "../labeled-input/LabeledInput";
import { Slider } from "../slider/Slider";

async function loadConfig() {
  try {
    const res = await fetch("/api/config", { method: "GET" });
    return await res.json();
  } catch (error) {
    console.error("Failed to load config:", error);
  }
}

async function saveConfig(config: DeepPartial<Config>) {
  try {
    await fetch("/api/config", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(config),
    });
  } catch (error) {
    console.error("Failed to save config:", error);
  }
}

export function DmxForm() {
  const [config, setConfig] = useState<Config | null>(null);
  const [changedFields, setChangedFields] = useState<DeepPartial<Config>>({});

  const updateConfig = (updatedFields: DeepPartial<Config>) => {
    setConfig((prevConfig) => {
      if (!prevConfig) return prevConfig;
      return deepMerged(prevConfig, updatedFields);
    });
    setChangedFields((prev) => deepMerged(prev, updatedFields));
  };

  const onSubmit = async (event: Event) => {
    event.preventDefault();
    await saveConfig(changedFields);
  };

  useEffect(() => {
    loadConfig().then((data) => {
      console.log("Loaded config:", data);
      setConfig(data);
    });
  }, []);

  return (
    <form onSubmit={onSubmit}>
      <Fieldset legend="Connectivity">
        <LabeledInput label="Mode">
          <Dropdown
            name="mode"
            selectedValue={`${config?.connection ?? 0}`}
            options={[
              { label: "Access Point", value: "0" },
              { label: "Station (not yet implemented)", value: "1", disabled: true },
              { label: "Ethernet (not yet implemented)", value: "2", disabled: true },
            ]}
            onValueChange={(value) => updateConfig({ connection: Number.parseInt(value) })}
          />
        </LabeledInput>

        <LabeledInput label="WiFi-SSID">
          <input
            type="text"
            name="ssid"
            value={config?.ap_config.ssid ?? ""}
            onInput={(e) => updateConfig({ ap_config: { ssid: e.currentTarget.value } })}
            required
          />
        </LabeledInput>

        <LabeledInput label="Password">
          <input
            type="password"
            name="password"
            value={
              config?.connection === 0
                ? config.ap_config.password
                : (config?.station_config.password ?? "")
            }
            onInput={(e) => updateConfig({ ap_config: { password: e.currentTarget.value } })}
          />
        </LabeledInput>
      </Fieldset>

      <Fieldset legend="DMX">
        {Object.entries(config?.dmx_ports || {}).map(([index, port]) => (
          <Fragment key={index}>
            <LabeledInput label={`DMX-Port ${index}`}>
              <Dropdown
                name={`dmx-${index}`}
                selectedValue={`${port.direction}`}
                options={[0, 1].map((value) => ({
                  label: value === 0 ? "Input" : "Output",
                  value: `${value}`,
                }))}
                onValueChange={(value) => {
                  updateConfig({ dmx_ports: { [index]: { direction: Number.parseInt(value) } } });
                }}
              />
            </LabeledInput>
            <LabeledInput label={`Art-Net Universe for Port ${index}`}>
              <input
                type="number"
                name={`universe-${index}`}
                value={port.universe}
                min={0}
                max={32767}
                onInput={(event) => {
                  updateConfig({
                    dmx_ports: {
                      [index]: { universe: Number.parseInt(event.currentTarget.value) },
                    },
                  });
                }}
                required
              />
            </LabeledInput>
          </Fragment>
        ))}
      </Fieldset>

      <Fieldset legend="Button & LED">
        <LabeledInput label="LED Brightness">
          <Slider
            name="brightness"
            value={config?.led_brightness}
            min={0}
            max={255}
            relative={true}
            onValueChange={(value) => {
              updateConfig({ led_brightness: value });
            }}
          />
        </LabeledInput>
      </Fieldset>

      <button type="submit" class="centered">
        Save
      </button>
    </form>
  );
}
