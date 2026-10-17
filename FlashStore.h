#ifndef FLASH_STORE_H
#define FLASH_STORE_H

#include "pxt.h"

#if MICROBIT_CODAL

#include "MicroBit.h"
#include "MicroBitFlash.h"
#include <stdint.h>

/*
 * FlashStore — хранение данных во flash micro:bit v2 между перезапусками.
 *
 * Ключ-значение: ключ до 15 символов, значение до 1024 байт.
 * Данные лежат в двух страницах flash прямо под scratch-страницей CODAL
 * (MICROBIT_DEFAULT_SCRATCH_PAGE). Запись добавляется в конец страницы,
 * когда место кончается — актуальные записи переезжают на вторую страницу.
 * Если питание пропадёт посреди записи, старые данные останутся целыми.
 *
 * Данные переживают перезагрузку и выключение питания. Прошивка новой
 * программы по USB может их стереть (так устроен загрузчик micro:bit).
 *
 * Страницы можно переопределить при сборке: -DFLASHSTORE_PAGE_A=0x... -DFLASHSTORE_PAGE_B=0x...
 * (адреса должны быть выровнены по размеру страницы, 4096 байт).
 */

#define FLASHSTORE_MAX_KEY		15
#define FLASHSTORE_MAX_VALUE	1024

class FlashStore
{
	public:

	FlashStore();

	/**
	 * Записать значение.
	 * @param key имя (1..15 символов, строка с нулём на конце)
	 * @param data данные
	 * @param len длина данных в байтах (0..1024)
	 * @return MICROBIT_OK, MICROBIT_INVALID_PARAMETER или MICROBIT_NO_RESOURCES (не влезает)
	 */
	int write(const char* key, const void* data, int len);

	/**
	 * Прочитать значение.
	 * @param key имя
	 * @param buf куда копировать (может быть NULL, чтобы узнать только длину)
	 * @param maxLen размер buf
	 * @return полная длина сохранённого значения (в buf копируется не больше maxLen),
	 *         MICROBIT_NO_DATA если ключа нет, MICROBIT_INVALID_PARAMETER при плохом ключе
	 */
	int read(const char* key, void* buf, int maxLen);

	/**
	 * Удалить ключ.
	 * @return MICROBIT_OK или MICROBIT_NO_DATA, если такого ключа не было
	 */
	int remove(const char* key);

	/**
	 * Удалить всё.
	 */
	int clear();

	/**
	 * Записать значение любого простого типа (int, float, struct без указателей).
	 */
	template <typename T>
	int put(const char* key, const T& value)
	{
		return write(key, &value, sizeof(T));
	}

	/**
	 * Прочитать значение того же типа, что и при put().
	 * @return MICROBIT_OK, MICROBIT_NO_DATA или MICROBIT_INVALID_PARAMETER (размер не совпал)
	 */
	template <typename T>
	int get(const char* key, T& value)
	{
		int len = read(key, NULL, 0);
		if (len < 0)
			return len;
		if (len != (int)sizeof(T))
			return MICROBIT_INVALID_PARAMETER;
		return read(key, &value, sizeof(T)) < 0 ? MICROBIT_NO_DATA : MICROBIT_OK;
	}

	private:

	struct Rec
	{
		uint32_t offset;
		uint32_t size;
		uint32_t keyLen;
		uint32_t valLen;
		bool tombstone;
		const uint8_t* key;
		const uint8_t* value;
	};

	codal::MicroBitFlash flash;
	uint32_t pageSize;
	uint32_t pageAddr[2];
	uint32_t seq;
	uint32_t freeOffset;
	int active;
	bool ready;

	int init();
	int parse(const uint8_t* page, uint32_t offset, Rec& rec) const;
	bool findLast(const char* key, Rec& rec) const;
	bool pageValid(int index, uint32_t& pageSeq) const;
	const uint8_t* activePage() const;
	int append(const char* key, const void* data, int len, bool tombstone);
	int compact(const char* key, const void* data, int len, bool tombstone, bool dropAll);

	static uint32_t align4(uint32_t n);
	static uint32_t encode(uint8_t* out, const char* key, const void* data, int len, bool tombstone);
};

#endif // MICROBIT_CODAL

#endif
