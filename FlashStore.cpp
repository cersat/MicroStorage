#include "FlashStore.h"

#if MICROBIT_CODAL

#include "MicroBitConfig.h"
#include <string.h>
#include <stdlib.h>

/*
 * Формат страницы (всё выровнено по 4 байта):
 *
 *   [magic 4][seq 4] [запись] [запись] ... [0xFF 0xFF ...]
 *
 * Запись:
 *   байт 0   длина ключа (1..15)
 *   байт 1,2 длина значения (little-endian), 0xFFFF = ключ удалён
 *   байт 3   контрольная сумма (XOR всех байт ключа и значения и первых трёх байт заголовка, ^ 0x5A)
 *   ключ, добитый до 4 байт
 *   значение, добитое до 4 байт
 *
 * Внутри записи заголовок пишется после тела, внутри страницы — после всех записей,
 * поэтому оборванная запись никогда не выглядит как целая.
 *
 * Актуальна та из двух страниц, у которой корректный magic и больший seq.
 * Заголовок новой страницы пишется ПОСЛЕДНИМ, поэтому недописанная
 * страница при сбое питания просто игнорируется.
 */

#define FS_MAGIC		0x3153564BUL	// "KVS1"
#define FS_HEADER		8
#define FS_TOMBSTONE	0xFFFF

#ifndef FLASHSTORE_PAGE_B
#define FLASHSTORE_PAGE_B	(MICROBIT_DEFAULT_SCRATCH_PAGE - MICROBIT_CODEPAGESIZE)
#endif

#ifndef FLASHSTORE_PAGE_A
#define FLASHSTORE_PAGE_A	(MICROBIT_DEFAULT_SCRATCH_PAGE - 2 * MICROBIT_CODEPAGESIZE)
#endif

static uint32_t* flashPtr(uint32_t address)
{
	return (uint32_t*)(uintptr_t)address;
}

static uint8_t checksum(const uint8_t* rec, uint32_t keyLen, uint32_t valLen)
{
	uint8_t sum = 0x5A ^ rec[0] ^ rec[1] ^ rec[2];
	const uint8_t* key = rec + 4;
	const uint8_t* value = key + ((keyLen + 3) & ~3UL);

	for (uint32_t i = 0; i < keyLen; i++)
		sum ^= key[i];
	for (uint32_t i = 0; i < valLen; i++)
		sum ^= value[i];
	return sum;
}

FlashStore::FlashStore()
{
	pageSize = 0;
	pageAddr[0] = 0;
	pageAddr[1] = 0;
	seq = 0;
	freeOffset = 0;
	active = 0;
	ready = false;
}

uint32_t FlashStore::align4(uint32_t n)
{
	return (n + 3) & ~3UL;
}

uint32_t FlashStore::encode(uint8_t* out, const char* key, const void* data, int len, bool tombstone)
{
	uint32_t keyLen = strlen(key);
	uint32_t valLen = tombstone ? 0 : (uint32_t)len;
	uint32_t keySize = align4(keyLen);
	uint32_t valSize = align4(valLen);
	uint32_t field = tombstone ? FS_TOMBSTONE : valLen;

	memset(out, 0, 4 + keySize + valSize);
	out[0] = (uint8_t)keyLen;
	out[1] = (uint8_t)(field & 0xFF);
	out[2] = (uint8_t)(field >> 8);
	memcpy(out + 4, key, keyLen);
	if (valLen)
		memcpy(out + 4 + keySize, data, valLen);
	out[3] = checksum(out, keyLen, valLen);

	return 4 + keySize + valSize;
}

/*
 * 1 — запись корректна, 0 — дальше чистая память (конец), -1 — запись повреждена.
 */
int FlashStore::parse(const uint8_t* page, uint32_t offset, Rec& rec) const
{
	if (offset + 4 > pageSize)
		return 0;

	const uint8_t* p = page + offset;

	if (p[0] == 0xFF && p[1] == 0xFF && p[2] == 0xFF && p[3] == 0xFF)
		return 0;

	uint32_t keyLen = p[0];
	uint32_t field = p[1] | ((uint32_t)p[2] << 8);
	bool tombstone = field == FS_TOMBSTONE;
	uint32_t valLen = tombstone ? 0 : field;

	if (keyLen < 1 || keyLen > FLASHSTORE_MAX_KEY || valLen > FLASHSTORE_MAX_VALUE)
		return -1;

	uint32_t size = 4 + align4(keyLen) + align4(valLen);

	if (offset + size > pageSize)
		return -1;
	if (checksum(p, keyLen, valLen) != p[3])
		return -1;

	rec.offset = offset;
	rec.size = size;
	rec.keyLen = keyLen;
	rec.valLen = valLen;
	rec.tombstone = tombstone;
	rec.key = p + 4;
	rec.value = p + 4 + align4(keyLen);
	return 1;
}

bool FlashStore::pageValid(int index, uint32_t& pageSeq) const
{
	const uint32_t* header = flashPtr(pageAddr[index]);

	if (header[0] != FS_MAGIC || header[1] == 0xFFFFFFFFUL)
		return false;

	pageSeq = header[1];
	return true;
}

const uint8_t* FlashStore::activePage() const
{
	return (const uint8_t*)(uintptr_t)pageAddr[active];
}

int FlashStore::init()
{
	pageSize = MICROBIT_CODEPAGESIZE;
	pageAddr[0] = FLASHSTORE_PAGE_A;
	pageAddr[1] = FLASHSTORE_PAGE_B;

	if ((pageAddr[0] % pageSize) || (pageAddr[1] % pageSize))
		return MICROBIT_INVALID_PARAMETER;

	// страницы не должны залезать на саму программу
	if (pageAddr[0] < FLASH_PROGRAM_END)
		return MICROBIT_NO_RESOURCES;

	uint32_t seq0 = 0, seq1 = 0;
	bool valid0 = pageValid(0, seq0);
	bool valid1 = pageValid(1, seq1);

	if (!valid0 && !valid1)
	{
		// первый запуск: чистим обе страницы и делаем первую активной
		flash.erase_page(flashPtr(pageAddr[0]));
		flash.erase_page(flashPtr(pageAddr[1]));

		uint32_t header[2] = { FS_MAGIC, 1 };
		flash.flash_burn(flashPtr(pageAddr[0]), header, 2);

		active = 0;
		seq = 1;
		freeOffset = FS_HEADER;
		ready = true;
		return MICROBIT_OK;
	}

	active = (valid0 && (!valid1 || seq0 > seq1)) ? 0 : 1;
	seq = active == 0 ? seq0 : seq1;

	Rec rec;
	uint32_t offset = FS_HEADER;
	int rc;

	while ((rc = parse(activePage(), offset, rec)) == 1)
		offset += rec.size;

	// после последней записи должна быть чистая память; если нет — оборвалась запись тела
	if (rc == 0)
	{
		for (uint32_t i = offset; i < pageSize; i++)
		{
			if (activePage()[i] != 0xFF)
			{
				rc = -1;
				break;
			}
		}
	}

	freeOffset = offset;
	ready = true;

	// хвост повреждён (оборвалась запись) — переносим целые записи на чистую страницу
	if (rc < 0)
		return compact(NULL, NULL, 0, false, false);

	return MICROBIT_OK;
}

/*
 * Ищет последнюю запись с этим ключом. true — если ключ существует и не удалён.
 */
bool FlashStore::findLast(const char* key, Rec& rec) const
{
	uint32_t keyLen = strlen(key);
	uint32_t offset = FS_HEADER;
	Rec cur;
	bool found = false;

	while (parse(activePage(), offset, cur) == 1)
	{
		offset += cur.size;

		if (cur.keyLen == keyLen && memcmp(cur.key, key, keyLen) == 0)
		{
			rec = cur;
			found = !cur.tombstone;
		}
	}

	return found;
}

int FlashStore::append(const char* key, const void* data, int len, bool tombstone)
{
	uint32_t need = 4 + align4(strlen(key)) + align4(tombstone ? 0 : (uint32_t)len);

	if (freeOffset + need > pageSize)
		return compact(key, data, len, tombstone, false);

	uint32_t* buf = (uint32_t*)malloc(need);

	if (!buf)
		return MICROBIT_NO_RESOURCES;

	encode((uint8_t*)buf, key, data, len, tombstone);

	// сначала тело записи, заголовок — последним: пока его нет, запись не существует
	uint32_t* dest = flashPtr(pageAddr[active] + freeOffset);

	if (need > 4)
		flash.flash_burn(dest + 1, buf + 1, need / 4 - 1);
	flash.flash_burn(dest, buf, 1);
	free(buf);

	freeOffset += need;
	return MICROBIT_OK;
}

/*
 * Собирает в RAM образ новой страницы: все актуальные записи + (если задана) новая,
 * и только потом стирает вторую страницу и пишет образ в неё.
 * Если не влезает — возвращает MICROBIT_NO_RESOURCES, ничего не стирая.
 */
int FlashStore::compact(const char* key, const void* data, int len, bool tombstone, bool dropAll)
{
	uint32_t* image = (uint32_t*)malloc(pageSize);

	if (!image)
		return MICROBIT_NO_RESOURCES;

	const uint8_t* src = activePage();
	uint8_t* out = (uint8_t*)image;
	uint32_t pos = FS_HEADER;
	uint32_t pendingLen = key ? strlen(key) : 0;

	memset(out, 0xFF, pageSize);

	if (!dropAll)
	{
		Rec rec, later;
		uint32_t offset = FS_HEADER;

		while (parse(src, offset, rec) == 1)
		{
			offset += rec.size;

			if (rec.tombstone)
				continue;

			// эту запись заменит новая
			if (key && rec.keyLen == pendingLen && memcmp(rec.key, key, pendingLen) == 0)
				continue;

			// дальше в странице есть более свежая запись с тем же ключом
			bool superseded = false;
			uint32_t next = offset;

			while (parse(src, next, later) == 1)
			{
				next += later.size;

				if (later.keyLen == rec.keyLen && memcmp(later.key, rec.key, rec.keyLen) == 0)
				{
					superseded = true;
					break;
				}
			}

			if (superseded)
				continue;

			if (pos + rec.size > pageSize)
			{
				free(image);
				return MICROBIT_NO_RESOURCES;
			}

			memcpy(out + pos, src + rec.offset, rec.size);
			pos += rec.size;
		}
	}

	if (key && !tombstone)
	{
		uint32_t need = 4 + align4(pendingLen) + align4((uint32_t)len);

		if (pos + need > pageSize)
		{
			free(image);
			return MICROBIT_NO_RESOURCES;
		}

		encode(out + pos, key, data, len, false);
		pos += need;
	}

	int target = 1 - active;

	flash.erase_page(flashPtr(pageAddr[target]));

	if (pos > FS_HEADER)
		flash.flash_burn(flashPtr(pageAddr[target] + FS_HEADER), image + FS_HEADER / 4, (pos - FS_HEADER) / 4);

	// заголовок — последним: пока его нет, страница считается пустой
	uint32_t header[2] = { FS_MAGIC, seq + 1 };
	flash.flash_burn(flashPtr(pageAddr[target]), header, 2);

	free(image);

	seq++;
	active = target;
	freeOffset = pos;
	return MICROBIT_OK;
}

int FlashStore::write(const char* key, const void* data, int len)
{
	if (!key)
		return MICROBIT_INVALID_PARAMETER;

	size_t keyLen = strlen(key);

	if (keyLen < 1 || keyLen > FLASHSTORE_MAX_KEY)
		return MICROBIT_INVALID_PARAMETER;
	if (len < 0 || len > FLASHSTORE_MAX_VALUE || (len > 0 && !data))
		return MICROBIT_INVALID_PARAMETER;

	if (!ready)
	{
		int rc = init();

		if (rc != MICROBIT_OK)
			return rc;
	}

	// то же самое уже лежит во flash — не тратим ресурс памяти
	Rec rec;

	if (findLast(key, rec) && rec.valLen == (uint32_t)len && (len == 0 || memcmp(rec.value, data, len) == 0))
		return MICROBIT_OK;

	return append(key, data, len, false);
}

int FlashStore::read(const char* key, void* buf, int maxLen)
{
	if (!key || maxLen < 0)
		return MICROBIT_INVALID_PARAMETER;

	size_t keyLen = strlen(key);

	if (keyLen < 1 || keyLen > FLASHSTORE_MAX_KEY)
		return MICROBIT_INVALID_PARAMETER;

	if (!ready)
	{
		int rc = init();

		if (rc != MICROBIT_OK)
			return rc;
	}

	Rec rec;

	if (!findLast(key, rec))
		return MICROBIT_NO_DATA;

	if (buf && maxLen > 0 && rec.valLen > 0)
		memcpy(buf, rec.value, rec.valLen < (uint32_t)maxLen ? rec.valLen : (uint32_t)maxLen);

	return (int)rec.valLen;
}

int FlashStore::remove(const char* key)
{
	if (!key)
		return MICROBIT_INVALID_PARAMETER;

	size_t keyLen = strlen(key);

	if (keyLen < 1 || keyLen > FLASHSTORE_MAX_KEY)
		return MICROBIT_INVALID_PARAMETER;

	if (!ready)
	{
		int rc = init();

		if (rc != MICROBIT_OK)
			return rc;
	}

	Rec rec;

	if (!findLast(key, rec))
		return MICROBIT_NO_DATA;

	return append(key, NULL, 0, true);
}

int FlashStore::clear()
{
	if (!ready)
	{
		int rc = init();

		if (rc != MICROBIT_OK)
			return rc;
	}

	return compact(NULL, NULL, 0, false, true);
}

#endif // MICROBIT_CODAL
