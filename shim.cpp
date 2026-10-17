#include "pxt.h"
#include "FlashStore.h"

using namespace pxt;

namespace flashstore {

#if MICROBIT_CODAL
	static FlashStore store;
#endif

	// micro:bit v1 не поддерживается: там функции только возвращают код ошибки,
	// чтобы расширение компилировалось в универсальный hex.
	#define FLASHSTORE_NOT_SUPPORTED -1006

	//%
	int writeRaw(String key, String value)
	{
#if MICROBIT_CODAL
		if (!key || !value)
			return MICROBIT_INVALID_PARAMETER;

		return store.write(key->getUTF8Data(), value->getUTF8Data(), value->getUTF8Size());
#else
		return FLASHSTORE_NOT_SUPPORTED;
#endif
	}

	//%
	String readRaw(String key)
	{
#if MICROBIT_CODAL
		if (!key)
			return mkString("", 0);

		int len = store.read(key->getUTF8Data(), NULL, 0);

		if (len <= 0)
			return mkString("", 0);

		char* buf = (char*)malloc(len);

		if (!buf)
			return mkString("", 0);

		store.read(key->getUTF8Data(), buf, len);

		String result = mkString(buf, len);

		free(buf);
		return result;
#else
		return mkString("", 0);
#endif
	}

	//%
	int hasKey(String key)
	{
#if MICROBIT_CODAL
		if (!key)
			return 0;

		return store.read(key->getUTF8Data(), NULL, 0) >= 0 ? 1 : 0;
#else
		return 0;
#endif
	}

	//%
	int removeKey(String key)
	{
#if MICROBIT_CODAL
		if (!key)
			return MICROBIT_INVALID_PARAMETER;

		return store.remove(key->getUTF8Data());
#else
		return FLASHSTORE_NOT_SUPPORTED;
#endif
	}

	//%
	int clearAll()
	{
#if MICROBIT_CODAL
		return store.clear();
#else
		return FLASHSTORE_NOT_SUPPORTED;
#endif
	}
}
