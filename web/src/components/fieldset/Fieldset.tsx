import { ComponentChildren } from "preact";

import style from "./fieldset.module.scss";

type FieldsetProps = {
  legend: string;
  children?: ComponentChildren;
};

export function Fieldset({ legend, children }: FieldsetProps) {
  return (
    <fieldset class={style.fieldset}>
      <legend>{legend}</legend>
      <div>{children}</div>
    </fieldset>
  );
}
