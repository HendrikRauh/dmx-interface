export type DeepPartial<T> = {
  [P in keyof T]?: T[P] extends object ? DeepPartial<T[P]> : T[P];
};

export function deepMerged<T>(target: T, source: DeepPartial<T>): T {
  const output = { ...target };
  for (const key in source) {
    if (!Object.prototype.hasOwnProperty.call(source, key)) continue;

    const sourceValue = source[key];
    const targetValue = target[key];

    if (
      targetValue &&
      typeof targetValue === "object" &&
      !Array.isArray(targetValue) &&
      sourceValue &&
      typeof sourceValue === "object" &&
      !Array.isArray(sourceValue)
    ) {
      (output as any)[key] = deepMerged(targetValue, sourceValue);
    } else {
      (output as any)[key] = sourceValue;
    }
  }

  return output;
}
