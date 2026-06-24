import { CircleCheckIcon, CircleXIcon, InfoIcon, TriangleAlertIcon } from "lucide-preact";
import { ComponentChildren } from "preact";

import style from "./Callout.module.scss";

type CalloutProps = {
  type: "success" | "info" | "warning" | "error";
  title?: string;
  children: ComponentChildren;
};

export function Callout({ type, title, children }: CalloutProps) {
  const icons = {
    success: <CircleCheckIcon />,
    info: <InfoIcon />,
    warning: <TriangleAlertIcon />,
    error: <CircleXIcon />,
  };

  return (
    <div
      class={style.callout}
      data-type={type}
      data-layout={title ? "with-title" : "without-title"}
    >
      {icons[type]}
      {title && <p>{title}</p>}
      <div>{children}</div>
    </div>
  );
}
