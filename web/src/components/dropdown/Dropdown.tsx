type DropdownProps = {
  name: string;
  options: { label: string; value: string; disabled?: boolean }[];
  selectedValue?: string;
  required?: boolean;
  onValueChange?: (value: string) => void;
};

export function Dropdown({
  name,
  options,
  selectedValue = undefined,
  required = false,
  onValueChange = () => {},
}: DropdownProps) {
  return (
    <select name={name} required={required} onChange={(e) => onValueChange(e.currentTarget.value)}>
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
