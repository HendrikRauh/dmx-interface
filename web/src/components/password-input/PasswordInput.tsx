import { EyeIcon, EyeOffIcon } from "lucide-preact";
import { InputHTMLAttributes } from "preact";
import { useState } from "preact/hooks";

import style from "./PasswordInput.module.scss";

type PasswordInputProps = Omit<InputHTMLAttributes<HTMLInputElement>, "type">;

export function PasswordInput({ ...props }: PasswordInputProps) {
  const [showPassword, setShowPassword] = useState(false);

  return (
    <div class={style.passwordInput}>
      <input type={showPassword ? "text" : "password"} {...props} />
      <button
        type="button"
        onClick={() => setShowPassword((prevState) => !prevState)}
        aria-label={showPassword ? "Hide password" : "Show password"}
      >
        {showPassword ? <EyeOffIcon></EyeOffIcon> : <EyeIcon></EyeIcon>}
      </button>
    </div>
  );
}
