//% color="#2b7a78" icon="" block="Flash"
namespace flashstore {

	let lastCode = 0

	/**
	 * Записать текст во flash. Ключ — до 15 байт, значение — до 1024 байт.
	 */
	//% block="сохранить во flash %key = %value"
	//% weight=100
	export function writeString(key: string, value: string): void {
		lastCode = writeRaw(key, value)
	}

	/**
	 * Записать число во flash.
	 */
	//% block="сохранить во flash число %key = %value"
	//% weight=95
	export function writeNumber(key: string, value: number): void {
		lastCode = writeRaw(key, value.toString())
	}

	/**
	 * Прочитать текст из flash. Если ключа нет — пустая строка.
	 */
	//% block="прочитать из flash текст %key"
	//% weight=90
	export function readString(key: string): string {
		return readRaw(key)
	}

	/**
	 * Прочитать число из flash. Если ключа нет — значение по умолчанию.
	 */
	//% block="прочитать из flash число %key || по умолчанию %defaultValue"
	//% weight=85 defaultValue.defl=0
	export function readNumber(key: string, defaultValue: number = 0): number {
		if (hasKey(key) == 0)
			return defaultValue

		return parseFloat(readRaw(key))
	}

	/**
	 * Есть ли такой ключ во flash.
	 */
	//% block="во flash есть %key"
	//% weight=80
	export function exists(key: string): boolean {
		return hasKey(key) != 0
	}

	/**
	 * Удалить ключ.
	 */
	//% block="удалить из flash %key"
	//% weight=70
	export function remove(key: string): void {
		lastCode = removeKey(key)
	}

	/**
	 * Удалить всё.
	 */
	//% block="очистить flash"
	//% weight=60
	export function clear(): void {
		lastCode = clearAll()
	}

	/**
	 * Результат последней операции: 0 — успешно, отрицательное число — ошибка
	 * (например, не хватило места или ключа не было).
	 */
	//% block="код последней операции flash"
	//% weight=50
	export function lastResult(): number {
		return lastCode
	}
}
