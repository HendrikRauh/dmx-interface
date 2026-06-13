import style from "./Slider.module.scss";

type SliderProps = {
  name: string;
  value?: number;
  min?: number;
  max?: number;
  step?: number | string;
  relative?: boolean;
  onValueChange?: (value: number) => void;
};

export function Slider({
  name,
  value = 0,
  min = 0,
  max = 100,
  step,
  relative = false,
  onValueChange = () => {},
}: SliderProps) {
  const percentage = Math.round(((value - min) / (max - min)) * 100);

  return (
    <div class={style.slider}>
      <input
        type="range"
        value={value}
        name={name}
        min={min}
        max={max}
        step={step}
        onInput={(event) => {
          const newValue = Number.parseFloat(event.currentTarget.value);
          onValueChange(newValue);
        }}
      />
      {relative ? <span>{percentage}%</span> : <span>{value}</span>}
    </div>
  );
}
