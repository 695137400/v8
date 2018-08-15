// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INTL_SUPPORT
#error Internationalization is expected to be enabled.
#endif  // V8_INTL_SUPPORT

#include "src/objects/js-number-format.h"

#include "src/isolate.h"
#include "src/objects-inl.h"
#include "src/objects/intl-objects.h"
#include "src/objects/js-number-format-inl.h"
#include "unicode/decimfmt.h"
#include "unicode/locid.h"
#include "unicode/numfmt.h"
#include "unicode/strenum.h"
#include "unicode/ucurr.h"
#include "unicode/uloc.h"

namespace v8 {
namespace internal {

namespace {

// ecma-402/#sec-currencydigits
// The currency is expected to an all upper case string value.
int CurrencyDigits(const char16_t* currency) {
  UErrorCode status = U_ZERO_ERROR;
  uint32_t fraction_digits =
      ucurr_getDefaultFractionDigits((const UChar*)(currency), &status);
  // For missing currency codes, default to the most common, 2
  if (U_FAILURE(status)) fraction_digits = 2;
  return fraction_digits;
}

bool IsAToZ(char ch) { return IsInRange(AsciiAlphaToLower(ch), 'a', 'z'); }

// ecma402/#sec-iswellformedcurrencycode
bool IsWellFormedCurrencyCode(const std::string& currency) {
  // Verifies that the input is a well-formed ISO 4217 currency code.
  // ecma402/#sec-currency-codes
  // 2. If the number of elements in normalized is not 3, return false.
  if (currency.length() != 3) return false;
  // 1. Let normalized be the result of mapping currency to upper case as
  // described in 6.1. 3. If normalized contains any character that is not in
  // the range "A" to "Z" (U+0041 to U+005A), return false. 4. Return true.
  // Don't uppercase to test. It could convert invalid code into a valid one.
  // For example \u00DFP (Eszett+P) becomes SSP.
  return (IsAToZ(currency[0]) && IsAToZ(currency[1]) && IsAToZ(currency[2]));
}

std::string GetNumberingSystem(Isolate* isolate,
                               const icu::Locale& icu_locale) {
  // Ugly hack. ICU doesn't expose numbering system in any way, so we have
  // to assume that for given locale NumberingSystem constructor produces the
  // same digits as NumberFormat/Calendar would.
  UErrorCode status = U_ZERO_ERROR;
  icu::NumberingSystem* numbering_system =
      icu::NumberingSystem::createInstance(icu_locale, status);
  std::string result;
  if (U_SUCCESS(status)) {
    result = numbering_system->getName();
  }
  delete numbering_system;
  return result;
}

void CreateDataPropertyForOptions(Isolate* isolate, Handle<JSObject> options,
                                  Handle<String> key, int32_t value) {
  Handle<Object> value_obj = isolate->factory()->NewNumberFromInt(value);

  // This is a brand new JSObject that shouldn't already have the same
  // key so this shouldn't fail.
  CHECK(JSReceiver::CreateDataProperty(isolate, options, key, value_obj,
                                       kDontThrow)
            .FromJust());
}

void CreateDataPropertyForOptions(Isolate* isolate, Handle<JSObject> options,
                                  Handle<String> key, Handle<String> value) {
  // This is a brand new JSObject that shouldn't already have the same
  // key so this shouldn't fail.
  CHECK(JSReceiver::CreateDataProperty(isolate, options, key, value, kDontThrow)
            .FromJust());
}

void CreateDataPropertyForOptions(Isolate* isolate, Handle<JSObject> options,
                                  Handle<String> key, bool value) {
  Handle<Object> value_obj = isolate->factory()->ToBoolean(value);

  // This is a brand new JSObject that shouldn't already have the same
  // key so this shouldn't fail.
  CHECK(JSReceiver::CreateDataProperty(isolate, options, key, value_obj,
                                       kDontThrow)
            .FromJust());
}

}  // anonymous namespace

// static
Handle<JSObject> JSNumberFormat::ResolvedOptions(
    Isolate* isolate, Handle<JSNumberFormat> number_format_holder) {
  Factory* factory = isolate->factory();
  Handle<JSObject> options = factory->NewJSObject(isolate->object_function());
  CreateDataPropertyForOptions(isolate, options, factory->style_string(),
                               number_format_holder->StyleAsString());
  icu::NumberFormat* number_format =
      UnpackIcuNumberFormat(isolate, number_format_holder);
  icu::DecimalFormat* decimal_format =
      static_cast<icu::DecimalFormat*>(number_format);
  DCHECK_NOT_NULL(number_format);
  Handle<String> locale =
      Handle<String>(number_format_holder->locale(), isolate);
  CreateDataPropertyForOptions(isolate, options, factory->locale_string(),
                               locale);
  UErrorCode error = U_ZERO_ERROR;
  icu::Locale icu_locale = number_format->getLocale(ULOC_VALID_LOCALE, error);
  DCHECK(U_SUCCESS(error));
  std::string numbering_system = GetNumberingSystem(isolate, icu_locale);
  if (!numbering_system.empty()) {
    CreateDataPropertyForOptions(
        isolate, options, factory->numberingSystem_string(),
        factory->NewStringFromAsciiChecked(numbering_system.c_str()));
  }

  if (number_format_holder->style() == Style::CURRENCY) {
    CreateDataPropertyForOptions(
        isolate, options, factory->currencyDisplay_string(),
        number_format_holder->CurrencyDisplayAsString());
    icu::UnicodeString currency(number_format->getCurrency());
    DCHECK(!currency.isEmpty());
    CreateDataPropertyForOptions(
        isolate, options, factory->currency_string(),
        factory
            ->NewStringFromTwoByte(Vector<const uint16_t>(
                reinterpret_cast<const uint16_t*>(currency.getBuffer()),
                currency.length()))
            .ToHandleChecked());
  }

  CreateDataPropertyForOptions(isolate, options,
                               factory->minimumIntegerDigits_string(),
                               number_format->getMinimumIntegerDigits());
  CreateDataPropertyForOptions(isolate, options,
                               factory->minimumFractionDigits_string(),
                               number_format->getMinimumFractionDigits());
  CreateDataPropertyForOptions(isolate, options,
                               factory->maximumFractionDigits_string(),
                               number_format->getMaximumFractionDigits());
  if (decimal_format->areSignificantDigitsUsed()) {
    CreateDataPropertyForOptions(isolate, options,
                                 factory->minimumSignificantDigits_string(),
                                 decimal_format->getMinimumSignificantDigits());
    CreateDataPropertyForOptions(isolate, options,
                                 factory->maximumSignificantDigits_string(),
                                 decimal_format->getMaximumSignificantDigits());
  }
  CreateDataPropertyForOptions(isolate, options, factory->useGrouping_string(),
                               (number_format->isGroupingUsed() == TRUE));

  return options;
}

namespace {

std::map<const char*, const char*> LookupUnicodeExtensions(
    icu::Locale& icu_locale, std::set<const char*>& relevant_keys) {
  std::map<const char*, const char*> extensions;

  UErrorCode status = U_ZERO_ERROR;
  std::unique_ptr<icu::StringEnumeration> keywords(
      icu_locale.createKeywords(status));
  if (U_FAILURE(status)) return extensions;

  if (!keywords) return extensions;
  char value[ULOC_FULLNAME_CAPACITY];

  int32_t length;
  status = U_ZERO_ERROR;
  for (const char* keyword = keywords->next(&length, status);
       keyword != nullptr; keyword = keywords->next(&length, status)) {
    // Ignore failures in ICU and skip to the next keyword.
    //
    // This is fine.™
    if (U_FAILURE(status)) {
      status = U_ZERO_ERROR;
      continue;
    }

    icu_locale.getKeywordValue(keyword, value, ULOC_FULLNAME_CAPACITY, status);

    // Ignore failures in ICU and skip to the next keyword.
    //
    // This is fine.™
    if (U_FAILURE(status)) {
      status = U_ZERO_ERROR;
      continue;
    }

    const char* bcp47_key = uloc_toUnicodeLocaleKey(keyword);

    // Ignore keywords that we don't recognize - spec allows that.
    if (bcp47_key && (relevant_keys.find(bcp47_key) != relevant_keys.end())) {
      const char* bcp47_value = uloc_toUnicodeLocaleType(bcp47_key, value);
      extensions.insert(
          std::pair<const char*, const char*>(bcp47_key, bcp47_value));
    }
  }

  return extensions;
}

}  // anonymous namespace

// static
MaybeHandle<JSNumberFormat> JSNumberFormat::InitializeNumberFormat(
    Isolate* isolate, Handle<JSNumberFormat> number_format,
    Handle<Object> locales, Handle<Object> options_obj) {
  Factory* factory = isolate->factory();
  // 1. Let requestedLocales be ? CanonicalizeLocaleList(locales).
  Handle<JSObject> requested_locales;
  ASSIGN_RETURN_ON_EXCEPTION(isolate, requested_locales,
                             Intl::CanonicalizeLocaleListJS(isolate, locales),
                             JSNumberFormat);

  // 2. If options is undefined, then
  if (options_obj->IsUndefined(isolate)) {
    // 2. a. Let options be ObjectCreate(null).
    options_obj = isolate->factory()->NewJSObjectWithNullProto();
  } else {
    // 3. Else
    // 3. a. Let options be ? ToObject(options).
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, options_obj,
        Object::ToObject(isolate, options_obj, "Intl.NumberFormat"),
        JSNumberFormat);
  }

  // At this point, options_obj can either be a JSObject or a JSProxy only.
  Handle<JSReceiver> options = Handle<JSReceiver>::cast(options_obj);

  // 4. Let opt be a new Record.
  //
  // 5. Let matcher be ? GetOption(options, "localeMatcher", "string", «
  // "lookup", "best fit" », "best fit").
  //
  // 6. Set opt.[[localeMatcher]] to matcher.
  //
  // 7. Let localeData be %NumberFormat%.[[LocaleData]].
  //
  // 8. Let r be ResolveLocale(%NumberFormat%.[[AvailableLocales]],
  // requestedLocales, opt,  %NumberFormat%.[[RelevantExtensionKeys]],
  // localeData).
  //
  // 9. Set numberFormat.[[Locale]] to r.[[locale]].

  Handle<JSObject> r;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, r,
      Intl::ResolveLocale(isolate, "numberformat", requested_locales, options),
      JSNumberFormat);

  Handle<String> locale_with_extension_str =
      isolate->factory()->NewStringFromStaticChars("localeWithExtension");
  Handle<Object> locale_with_extension_obj =
      JSObject::GetDataProperty(r, locale_with_extension_str);

  // The locale_with_extension has to be a string. Either a user
  // provided canonicalized string or the default locale.
  CHECK(locale_with_extension_obj->IsString());
  Handle<String> locale_with_extension =
      Handle<String>::cast(locale_with_extension_obj);

  icu::Locale icu_locale =
      Intl::CreateICULocale(isolate, locale_with_extension);
  number_format->set_locale(*locale_with_extension);
  DCHECK(!icu_locale.isBogus());

  std::set<const char*> relevant_extension_keys{"nu"};
  std::map<const char*, const char*> extensions =
      LookupUnicodeExtensions(icu_locale, relevant_extension_keys);

  if (extensions.find("nu") != extensions.end()) {
    const char* value = extensions.at("nu");
    if (strcmp(value, "native") == 0 || strcmp(value, "traditio") == 0 ||
        strcmp(value, "finance") == 0) {
      UErrorCode status = U_ZERO_ERROR;
      icu_locale.setKeywordValue("nu", NULL, status);
      CHECK(U_SUCCESS(status));
    } else {
      UErrorCode status = U_ZERO_ERROR;
      icu_locale.setKeywordValue("nu", value, status);
      CHECK(U_SUCCESS(status));
    }
  }

  // 10. Set numberFormat.[[NumberingSystem]] to r.[[nu]].
  //
  // 11. Let dataLocale be r.[[dataLocale]].
  //
  // 12. Let style be ? GetOption(options, "style", "string",  « "decimal",
  // "percent", "currency" », "decimal").
  const char* service = "Intl.NumberFormat";
  std::unique_ptr<char[]> style_cstr;
  static std::vector<const char*> style_values = {"decimal", "percent",
                                                  "currency"};
  Maybe<bool> found_style = Intl::GetStringOption(
      isolate, options, "style", style_values, service, &style_cstr);
  MAYBE_RETURN(found_style, MaybeHandle<JSNumberFormat>());
  Style style = Style::DECIMAL;
  if (found_style.FromJust()) {
    if (strcmp(style_cstr.get(), "percent") == 0) {
      style = Style::PERCENT;
    } else if (strcmp(style_cstr.get(), "currency") == 0) {
      style = Style::CURRENCY;
    }
  }

  // 13. Set numberFormat.[[Style]] to style.
  number_format->set_style(style);

  // 14. Let currency be ? GetOption(options, "currency", "string", undefined,
  // undefined).
  std::unique_ptr<char[]> currency_cstr;
  static std::vector<const char*> empty_values = {};
  Maybe<bool> found_currency = Intl::GetStringOption(
      isolate, options, "currency", empty_values, service, &currency_cstr);
  MAYBE_RETURN(found_currency, MaybeHandle<JSNumberFormat>());

  std::string currency;
  // 15. If currency is not undefined, then
  if (found_currency.FromJust()) {
    DCHECK_NOT_NULL(currency_cstr.get());
    currency = currency_cstr.get();
    // 15. a. If the result of IsWellFormedCurrencyCode(currency) is false,
    // throw a RangeError exception.
    if (!IsWellFormedCurrencyCode(currency)) {
      THROW_NEW_ERROR_RETURN_VALUE(
          isolate,
          NewRangeError(MessageTemplate::kInvalidCurrencyCode,
                        factory->NewStringFromAsciiChecked(currency.c_str())),
          MaybeHandle<JSNumberFormat>());
    }
  }

  UErrorCode status = U_ZERO_ERROR;
  std::unique_ptr<icu::NumberFormat> icu_number_format;

  // 16. If style is "currency" and currency is undefined, throw a TypeError
  // exception.
  if (style == Style::CURRENCY && !found_currency.FromJust()) {
    THROW_NEW_ERROR_RETURN_VALUE(isolate,
                                 NewTypeError(MessageTemplate::kCurrencyCode),
                                 MaybeHandle<JSNumberFormat>());
  }
  // 17. If style is "currency", then
  if (style == Style::CURRENCY) {
    // a. Let currency be the result of converting currency to upper case as
    //    specified in 6.1
    std::transform(currency.begin(), currency.end(), currency.begin(), toupper);
  }

  // 18. Let currencyDisplay be ? GetOption(options, "currencyDisplay",
  // "string", « "code",  "symbol", "name" », "symbol").
  std::unique_ptr<char[]> currency_display_cstr;
  static std::vector<const char*> currency_display_values = {"code", "name",
                                                             "symbol"};
  Maybe<bool> found_currency_display = Intl::GetStringOption(
      isolate, options, "currencyDisplay", currency_display_values, service,
      &currency_display_cstr);
  MAYBE_RETURN(found_currency_display, MaybeHandle<JSNumberFormat>());
  CurrencyDisplay currency_display = CurrencyDisplay::SYMBOL;
  UNumberFormatStyle format_style = UNUM_CURRENCY;

  if (found_currency_display.FromJust()) {
    if (strcmp(currency_display_cstr.get(), "code") == 0) {
      currency_display = CurrencyDisplay::CODE;
      format_style = UNUM_CURRENCY_ISO;
    } else if (strcmp(currency_display_cstr.get(), "name") == 0) {
      currency_display = CurrencyDisplay::NAME;
      format_style = UNUM_CURRENCY_PLURAL;
    }
  }

  if (style == Style::DECIMAL) {
    icu_number_format.reset(
        icu::NumberFormat::createInstance(icu_locale, status));
  } else if (style == Style::PERCENT) {
    icu_number_format.reset(
        icu::NumberFormat::createPercentInstance(icu_locale, status));
  } else {
    DCHECK_EQ(style, Style::CURRENCY);
    icu_number_format.reset(
        icu::NumberFormat::createInstance(icu_locale, format_style, status));
  }

  if (U_FAILURE(status) || icu_number_format.get() == nullptr) {
    status = U_ZERO_ERROR;
    // Remove extensions and try again.
    icu::Locale no_extension_locale(icu_locale.getBaseName());
    icu_number_format.reset(
        icu::NumberFormat::createInstance(no_extension_locale, status));

    if (U_FAILURE(status) || icu_number_format.get() == nullptr) {
      FATAL("Failed to create ICU number_format, are ICU data files missing?");
    }
  }
  DCHECK(U_SUCCESS(status));
  CHECK_NOT_NULL(icu_number_format.get());
  if (style == Style::PERCENT) {
    icu_number_format->setMinimumFractionDigits(1);
  } else if (style == Style::CURRENCY) {
    // 19. If style is "currency", set  numberFormat.[[CurrencyDisplay]] to
    // currencyDisplay.
    number_format->set_currency_display(currency_display);
    // b. Set numberFormat.[[Currency]] to currency.
    icu::UnicodeString currency_ustr(currency.c_str());
    if (!currency_ustr.isEmpty()) {
      icu_number_format->setCurrency(currency_ustr.getBuffer(), status);
    }
  }

  // c. Let cDigits be CurrencyDigits(currency).
  icu::UnicodeString currency_ustr(currency.c_str());
  int c_digits = CurrencyDigits(currency_ustr.getBuffer());

  // 20. If style is "currency", then
  int mnfd_default;
  int mxfd_default;
  if (style == Style::CURRENCY) {
    //  a. Let mnfdDefault be cDigits.
    //  b. Let mxfdDefault be cDigits.
    mnfd_default = c_digits;
    mxfd_default = c_digits;
  } else {
    // 21. Else,
    // a. Let mnfdDefault be 0.
    mnfd_default = 0;
    // b. If style is "percent", then
    if (style == Style::PERCENT) {
      // i. Let mxfdDefault be 0.
      mxfd_default = 0;
    } else {
      // c. Else,
      // i. Let mxfdDefault be 3.
      mxfd_default = 3;
    }
  }
  // 22. Perform ? SetNumberFormatDigitOptions(numberFormat, options,
  // mnfdDefault, mxfdDefault).
  icu::DecimalFormat* icu_decimal_format =
      static_cast<icu::DecimalFormat*>(icu_number_format.get());
  Maybe<bool> maybe_set_number_for_digit_options =
      Intl::SetNumberFormatDigitOptions(isolate, icu_decimal_format, options,
                                        mnfd_default, mxfd_default);
  MAYBE_RETURN(maybe_set_number_for_digit_options, Handle<JSNumberFormat>());

  // 23. Let useGrouping be ? GetOption(options, "useGrouping", "boolean",
  // undefined, true).
  bool use_grouping = true;
  Maybe<bool> found_use_grouping = Intl::GetBoolOption(
      isolate, options, "useGrouping", service, &use_grouping);
  MAYBE_RETURN(found_use_grouping, MaybeHandle<JSNumberFormat>());
  // 24. Set numberFormat.[[UseGrouping]] to useGrouping.
  icu_number_format->setGroupingUsed(use_grouping ? TRUE : FALSE);

  // 25. Let dataLocaleData be localeData.[[<dataLocale>]].
  //
  // 26. Let patterns be dataLocaleData.[[patterns]].
  //
  // 27. Assert: patterns is a record (see 11.3.3).
  //
  // 28. Let stylePatterns be patterns.[[<style>]].
  //
  // 29. Set numberFormat.[[PositivePattern]] to
  // stylePatterns.[[positivePattern]].
  //
  // 30. Set numberFormat.[[NegativePattern]] to
  // stylePatterns.[[negativePattern]].

  Handle<Managed<icu::NumberFormat>> managed_number_format =
      Managed<icu::NumberFormat>::FromUniquePtr(isolate, 0,
                                                std::move(icu_number_format));
  number_format->set_icu_number_format(*managed_number_format);
  number_format->set_bound_format(*factory->undefined_value());

  // 31. Return numberFormat.
  return number_format;
}

icu::NumberFormat* JSNumberFormat::UnpackIcuNumberFormat(
    Isolate* isolate, Handle<JSNumberFormat> holder) {
  return Managed<icu::NumberFormat>::cast(holder->icu_number_format())->raw();
}

Handle<String> JSNumberFormat::StyleAsString() const {
  switch (style()) {
    case Style::DECIMAL:
      return GetReadOnlyRoots().decimal_string_handle();
    case Style::PERCENT:
      return GetReadOnlyRoots().percent_string_handle();
    case Style::CURRENCY:
      return GetReadOnlyRoots().currency_string_handle();
    case Style::COUNT:
      UNREACHABLE();
  }
}

Handle<String> JSNumberFormat::CurrencyDisplayAsString() const {
  switch (currency_display()) {
    case CurrencyDisplay::CODE:
      return GetReadOnlyRoots().code_string_handle();
    case CurrencyDisplay::SYMBOL:
      return GetReadOnlyRoots().symbol_string_handle();
    case CurrencyDisplay::NAME:
      return GetReadOnlyRoots().name_string_handle();
    case CurrencyDisplay::COUNT:
      UNREACHABLE();
  }
}

MaybeHandle<String> JSNumberFormat::FormatNumber(
    Isolate* isolate, Handle<JSNumberFormat> number_format_holder,
    double number) {
  icu::NumberFormat* number_format =
      UnpackIcuNumberFormat(isolate, number_format_holder);
  CHECK_NOT_NULL(number_format);

  icu::UnicodeString result;
  number_format->format(number, result);

  return isolate->factory()->NewStringFromTwoByte(Vector<const uint16_t>(
      reinterpret_cast<const uint16_t*>(result.getBuffer()), result.length()));
}

}  // namespace internal
}  // namespace v8
