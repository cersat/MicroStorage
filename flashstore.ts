//% color="#2b7a78" icon="" block="Flash"
namespace flashstore {

	let lastCode = 0

	// Тела функций ниже работают только в симуляторе (в памяти, пропадают при перезапуске).
	// На плате вызывается C++ из shim.cpp, и эти тела не используются.
	let simKeys: string[] = []
	let simValues: string[] = []

	function simIndex(key: string): number {
		return simKeys.indexOf(key)
	}

	//% shim=flashstore::writeRaw
	function writeRaw(key: string, value: string): number {
		if (key.length < 1 || key.length > 15 || value.length > 1024)
			return -1001

		let i = simIndex(key)

		if (i < 0) {
			simKeys.push(key)
			simValues.push(value)
		} else {
			simValues[i] = value
		}

		return 0
	}

	//% shim=flashstore::readRaw
	function readRaw(key: string): string {
		let i = simIndex(key)

		return i < 0 ? "" : simValues[i]
	}

	//% shim=flashstore::hasKey
	function hasKey(key: string): number {
		return simIndex(key) >= 0 ? 1 : 0
	}

	//% shim=flashstore::removeKey
	function removeKey(key: string): number {
		let i = simIndex(key)

		if (i < 0)
			return -1005

		simKeys.removeAt(i)
		simValues.removeAt(i)
		return 0
	}

	//% shim=flashstore::clearAll
	function clearAll(): number {
		simKeys = []
		simValues = []
		return 0
	}

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
