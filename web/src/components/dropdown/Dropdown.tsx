type DropdownProps = {
  name: string;
  options: { label: string; value: string; disabled?: boolean }[];
  selectedValue?: string;
  required?: boolean;
};

export function Dropdown({
  name,
  options,
  selectedValue = undefined,
  required = false,
}: DropdownProps) {
  return (
    <select name={name} required={required}>
      {options.map((option) => (
        <option
          key={option.value}
          value={option.value}
          disabled={option.disabled}
          selected={option.value === selectedValue}
        >
          {option.label}
        </option>
      ))}
    </select>
  );
}
