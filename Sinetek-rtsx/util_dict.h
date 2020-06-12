#pragma once

#define UTL_STATIC_DICT_INIT(dict_name) \
	template <> dict_name::key_t   dict_name::keys[] = { 0 }; \
	template <> dict_name::value_t dict_name::values[] = { 0 }

/*
 * This dictionary has the advantage that it does not need to be initialized at runtime.
 */
template <typename T_KEY, typename T_VAL, unsigned ARRAY_SIZE = 10> class StaticDictionary {
	static T_KEY keys[ARRAY_SIZE];
	static T_VAL values[ARRAY_SIZE];

	// returns -1 if no slots available
	static int getFreeSlot()
	{
		for (auto i = 0; i < ARRAY_SIZE; i++) {
			if (keys[i] == 0) {
				keys[i] = (T_KEY)-1; // mark as used
				return i;
			}
		}
		return -1; // no free slots
	}

	// returns -1 if not found
	static int findKey(T_KEY key)
	{
		for (auto i = 0; i < ARRAY_SIZE; i++) {
			if (keys[i] == key)
				return i;
		}
		return -1;
	}

public:
	using key_t = T_KEY;
	using value_t = T_VAL;
	// Update the list of virtual addres/segment (should keep track of all of them, but for now we only support one)
	static int addToList(T_KEY key, T_VAL val)
	{
		auto slotIdx = getFreeSlot();
		if (slotIdx < 0) {
			return -1;
		}
		keys[slotIdx] = key;
		values[slotIdx] = val;
		return 0;
	}

	static int removeFromList(T_KEY key)
	{
		auto slotIdx = findKey(key);
		if (slotIdx < 0)
			return -1;
		keys[slotIdx] = 0;
		values[slotIdx] = 0;
		return 0;
	}

	static int getValueFromList(T_KEY key, T_VAL *outVal)
	{
		auto slotIdx = findKey(key);
		if (slotIdx < 0)
			return -1;
		*outVal = values[slotIdx];
		return 0;
	}
};
