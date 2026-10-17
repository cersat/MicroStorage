#include "pxt.h"
#include "FlashStore.h"

using namespace pxt;

namespace flashstore {

	static FlashStore store;

	int writeRaw(String key, String value)
	{
		if (!key || !value)
			return MICROBIT_INVALID_PARAMETER;

		return store.write(key->getUTF8Data(), value->getUTF8Data(), value->getUTF8Size());
	}

	String readRaw(String key)
	{
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
	}

	int hasKey(String key)
	{
		if (!key)
			return 0;

		return store.read(key->getUTF8Data(), NULL, 0) >= 0 ? 1 : 0;
	}

	int removeKey(String key)
	{
		if (!key)
			return MICROBIT_INVALID_PARAMETER;

		return store.remove(key->getUTF8Data());
	}

	int clearAll()
	{
		return store.clear();
	}

}
