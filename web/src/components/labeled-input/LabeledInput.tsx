import { ComponentChildren } from "preact";

import style from "./LabeledInput.module.scss";

type LabeledInputProps = {
  label: string;
  children: ComponentChildren;
};

export function LabeledInput({ label, children }: LabeledInputProps) {
  return (
    <div class={style.labeledInput}>
      <label>
        <span>{label}:</span>
        {children}
      </label>
    </div>
  );
}
