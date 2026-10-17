//% color="#2b7a78" icon="" block="Flash"
namespace flashstore {

	let lastCode = 0

	// В симуляторе C++ не работает, поэтому там данные лежат в памяти и пропадают при перезапуске.
	let simKeys: string[] = []
	let simValues: string[] = []

	function inSimulator(): boolean {
		return control.deviceDalVersion() == "sim"
	}

	function simIndex(key: string): number {
		return simKeys.indexOf(key)
	}

	function validKey(key: string): boolean {
		return key.length >= 1 && key.length <= 15
	}

	function rawWrite(key: string, value: string): number {
		if (!inSimulator())
			return writeRaw(key, value)

		if (!validKey(key) || value.length > 1024)
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

	function rawRead(key: string): string {
		if (!inSimulator())
			return readRaw(key)

		let i = simIndex(key)

		return i < 0 ? "" : simValues[i]
	}

	function rawHas(key: string): boolean {
		if (!inSimulator())
			return hasKey(key) != 0

		return simIndex(key) >= 0
	}

	function rawRemove(key: string): number {
		if (!inSimulator())
			return removeKey(key)

		let i = simIndex(key)

		if (i < 0)
			return -1005

		simKeys.removeAt(i)
		simValues.removeAt(i)
		return 0
	}

	function rawClear(): number {
		if (!inSimulator())
			return clearAll()

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
		lastCode = rawWrite(key, value)
	}

	/**
	 * Записать число во flash.
	 */
	//% block="сохранить во flash число %key = %value"
	//% weight=95
	export function writeNumber(key: string, value: number): void {
		lastCode = rawWrite(key, value.toString())
	}

	/**
	 * Прочитать текст из flash. Если ключа нет — пустая строка.
	 */
	//% block="прочитать из flash текст %key"
	//% weight=90
	export function readString(key: string): string {
		return rawRead(key)
	}

	/**
	 * Прочитать число из flash. Если ключа нет — значение по умолчанию.
	 */
	//% block="прочитать из flash число %key || по умолчанию %defaultValue"
	//% weight=85 defaultValue.defl=0
	export function readNumber(key: string, defaultValue: number = 0): number {
		if (!rawHas(key))
			return defaultValue

		return parseFloat(rawRead(key))
	}

	/**
	 * Есть ли такой ключ во flash.
	 */
	//% block="во flash есть %key"
	//% weight=80
	export function exists(key: string): boolean {
		return rawHas(key)
	}

	/**
	 * Удалить ключ.
	 */
	//% block="удалить из flash %key"
	//% weight=70
	export function remove(key: string): void {
		lastCode = rawRemove(key)
	}

	/**
	 * Удалить всё.
	 */
	//% block="очистить flash"
	//% weight=60
	export function clear(): void {
		lastCode = rawClear()
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
