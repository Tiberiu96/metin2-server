#ifndef __INC_METIN2_GAME_LOCALE_H__
#define __INC_METIN2_GAME_LOCALE_H__

extern "C"
{
	void locale_init(const char *filename);
	const char *locale_find(const char *string);

	extern int g_iUseLocale;

#define LC_TEXT(str) locale_find(str)
};

void locale_init_all_langs();
const char * locale_find_lang(const char * string, const char * lang);
#define LC_TEXT_LANG(str, lang) locale_find_lang(str, lang)

#endif
