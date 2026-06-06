import { useState } from "preact/hooks";

import style from "./Slider.module.scss";

type SliderProps = {
  name: string;
  value?: number;
  min?: number;
  max?: number;
  step?: number | string;
  relative?: boolean;
};

export function Slider({
  name,
  value = 0,
  min = 0,
  max = 100,
  step,
  relative = false,
}: SliderProps) {
  const [_value, setValue] = useState(value);
  return (
    <div class={style.slider}>
      <input
        type="range"
        value={_value}
        name={name}
        min={min}
        max={max}
        step={step}
        onInput={(event) => setValue(Number.parseInt(event.currentTarget.value))}
      />
      {relative ? <span>{Math.round((_value / (max - min)) * 100)}%</span> : <span>{_value}</span>}
    </div>
  );
}
