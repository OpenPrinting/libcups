//
// JWT API unit tests for CUPS.
//
// Copyright © 2023 by OpenPrinting.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#include "cups.h"
#include "jwt.h"
#include "test-internal.h"


//
// 'main()' - Main entry.
//

int					// O - Exit status
main(int  argc,				// I - Number of command-line arguments
     char *argv[])			// I - Command-line arguments
{
  int		i;			// Looping var
  cups_jwt_t	*jwt;			// JSON Web Token object


  if (argc == 1)
  {
    // Do unit tests...
    static const char * const examples[][2] =
    {					// JWT examples
      {
        "eyJ0eXAiOiJKV1QiLA0KICJhbGciOiJIUzI1NiJ9."
	    "eyJpc3MiOiJqb2UiLA0KICJleHAiOjEzMDA4MTkzODAsDQo"
	    "gImh0dHA6Ly9leGFtcGxlLmNvbS9pc19yb290Ijp0cnVlfQ."
	    "dBjftJeZ4CVP-mB92K27uhbUJU1p1r_wW1gFWFOEjXk",
	"{\"kty\":\"oct\","
	    "\"k\":\"AyM1SysPpbyDfgZld3umj1qzKObwVMkoqQ-EstJQLr_T-1qS0gZH75"
	    "aKtMN3Yj0iPS4hcgUuTwjAzZr1Z9CAow\"}"
      },
      {
        "eyJhbGciOiJSUzI1NiJ9."
            "eyJpc3MiOiJqb2UiLA0KICJleHAiOjEzMDA4MTkzODAsDQogImh0dHA6Ly9leGFt"
            "cGxlLmNvbS9pc19yb290Ijp0cnVlfQ."
            "cC4hiUPoj9Eetdgtv3hF80EGrhuB__dzERat0XF9g2VtQgr9PJbu3XOiZj5RZmh7"
            "AAuHIm4Bh-0Qc_lF5YKt_O8W2Fp5jujGbds9uJdbF9CUAr7t1dnZcAcQjbKBYNX4"
            "BAynRFdiuB--f_nZLgrnbyTyWzO75vRK5h6xBArLIARNPvkSjtQBMHlb1L07Qe7K"
            "0GarZRmB_eSN9383LcOLn6_dO--xi12jzDwusC-eOkHWEsqtFZESc6BfI7noOPqv"
            "hJ1phCnvWh6IeYI2w9QOYEUipUTI8np6LbgGY9Fs98rqVt5AXLIhWkWywlVmtVrB"
            "p0igcN_IoypGlUPQGe77Rw",
	"{\"kty\":\"RSA\","
            "\"n\":\"ofgWCuLjybRlzo0tZWJjNiuSfb4p4fAkd_wWJcyQoTbji9k0l8W26mPddx"
            "HmfHQp-Vaw-4qPCJrcS2mJPMEzP1Pt0Bm4d4QlL-yRT-SFd2lZS-pCgNMs"
            "D1W_YpRPEwOWvG6b32690r2jZ47soMZo9wGzjb_7OMg0LOL-bSf63kpaSH"
            "SXndS5z5rexMdbBYUsLA9e-KXBdQOS-UTo7WTBEMa2R2CapHg665xsmtdV"
            "MTBQY4uDZlxvb3qCo5ZwKh9kG4LT6_I5IhlJH7aGhyxXFvUK-DWNmoudF8"
            "NAco9_h9iaGNj8q2ethFkMLs91kzk2PAcDTW9gb54h4FRWyuXpoQ\","
            "\"e\":\"AQAB\","
            "\"d\":\"Eq5xpGnNCivDflJsRQBXHx1hdR1k6Ulwe2JZD50LpXyWPEAeP88vLNO97I"
            "jlA7_GQ5sLKMgvfTeXZx9SE-7YwVol2NXOoAJe46sui395IW_GO-pWJ1O0"
            "BkTGoVEn2bKVRUCgu-GjBVaYLU6f3l9kJfFNS3E0QbVdxzubSu3Mkqzjkn"
            "439X0M_V51gfpRLI9JYanrC4D4qAdGcopV_0ZHHzQlBjudU2QvXt4ehNYT"
            "CBr6XCLQUShb1juUO1ZdiYoFaFQT5Tw8bGUl_x_jTj3ccPDVZFD9pIuhLh"
            "BOneufuBiB4cS98l2SR_RQyGWSeWjnczT0QU91p1DhOVRuOopznQ\","
            "\"p\":\"4BzEEOtIpmVdVEZNCqS7baC4crd0pqnRH_5IB3jw3bcxGn6QLvnEtfdUdi"
            "YrqBdss1l58BQ3KhooKeQTa9AB0Hw_Py5PJdTJNPY8cQn7ouZ2KKDcmnPG"
            "BY5t7yLc1QlQ5xHdwW1VhvKn-nXqhJTBgIPgtldC-KDV5z-y2XDwGUc\","
            "\"q\":\"uQPEfgmVtjL0Uyyx88GZFF1fOunH3-7cepKmtH4pxhtCoHqpWmT8YAmZxa"
            "ewHgHAjLYsp1ZSe7zFYHj7C6ul7TjeLQeZD_YwD66t62wDmpe_HlB-TnBA"
            "-njbglfIsRLtXlnDzQkv5dTltRJ11BKBBypeeF6689rjcJIDEz9RWdc\","
            "\"dp\":\"BwKfV3Akq5_MFZDFZCnW-wzl-CCo83WoZvnLQwCTeDv8uzluRSnm71I3Q"
            "CLdhrqE2e9YkxvuxdBfpT_PI7Yz-FOKnu1R6HsJeDCjn12Sk3vmAktV2zb"
            "34MCdy7cpdTh_YVr7tss2u6vneTwrA86rZtu5Mbr1C1XsmvkxHQAdYo0\","
            "\"dq\":\"h_96-mK1R_7glhsum81dZxjTnYynPbZpHziZjeeHcXYsXaaMwkOlODsWa"
            "7I9xXDoRwbKgB719rrmI2oKr6N3Do9U0ajaHF-NKJnwgjMd2w9cjz3_-ky"
            "NlxAr2v4IKhGNpmM5iIgOS1VZnOZ68m6_pbLBSp3nssTdlqvd0tIiTHU\","
            "\"qi\":\"IYd7DHOhrWvxkwPQsRM2tOgrjbcrfvtQJipd-DlcxyVuuM9sQLdgjVk2o"
            "y26F0EmpScGLq2MowX7fhd_QJQ3ydy5cY7YIBi87w93IKLEdfnbJtoOPLU"
            "W0ITrJReOgo1cq9SbsxYawBgfp_gh6A5603k2-ZQwVK0JKSHuLFkuQ3U\""
            "}"
      },
      {
        "eyJhbGciOiJFUzI1NiJ9."
            "eyJpc3MiOiJqb2UiLA0KICJleHAiOjEzMDA4MTkzODAsDQogImh0dHA6Ly9leGFt"
            "cGxlLmNvbS9pc19yb290Ijp0cnVlfQ."
            "DtEhU3ljbEg8L38VWAfUAqOyKAM6-Xx-F4GawxaepmXFCgfTjDxw5djxLa8ISlSA"
            "pmWQxfKTUJqPP3-Kg6NU1Q",
	"{\"kty\":\"EC\","
            "\"crv\":\"P-256\","
            "\"x\":\"f83OJ3D2xF1Bg8vub9tLe1gHMzV76e8Tus9uPHvRVEU\","
            "\"y\":\"x_FEzRu9m36HLN_tue659LNpXW6pCyStikYjKIWI5a0\","
            "\"d\":\"jpsQnnGQmL-YBIffH1136cspYG6-0iY7X1fCE9-E9LI\""
            "}"
      }
    };
#if 0
    static const char * const types[] =	// Node types
    {
      "CUPS_JTYPE_NULL",		// Null value
      "CUPS_JTYPE_FALSE",		// Boolean false value
      "CUPS_JTYPE_TRUE",		// Boolean true value
      "CUPS_JTYPE_NUMBER",		// Number value
      "CUPS_JTYPE_STRING",		// String value
      "CUPS_JTYPE_ARRAY",		// Array value
      "CUPS_JTYPE_OBJECT",		// Object value
      "CUPS_JTYPE_KEY"			// Object key (string)
    };
#endif // 0

    testBegin("cupsJWTNew(NULL)");
    jwt = cupsJWTNew(NULL);
    testEnd(jwt != NULL);

    for (i = 0; i < (int)(sizeof(examples) / sizeof(examples[0])); i ++)
    {
      cups_json_t	*jwk = NULL;	// JSON Web Key Set

      testBegin("cupsJWTImportString(\"%s\")", examples[i][0]);
      if ((jwt = cupsJWTImportString(examples[i][0])) != NULL)
      {
        testEnd(true);

        testBegin("cupsJSONImportString(\"%s\")", examples[i][1]);
        if ((jwk = cupsJSONImportString(examples[i][1])) != NULL)
        {
          testEnd(true);
          testBegin("cupsJWTHasValidSignature()");
          testEnd(cupsJWTHasValidSignature(jwt, jwk));
        }
        else
        {
          testEndMessage(false, "%s", cupsLastErrorString());
        }
      }
      else
      {
        testEndMessage(false, "%s", cupsLastErrorString());
      }

      cupsJSONDelete(jwk);
      cupsJWTDelete(jwt);
    }

#if 0
    testBegin("cupsJSONGetCount(root)");
    count = cupsJSONGetCount(json);
    testEndMessage(count == 0, "%u", (unsigned)count);

    testBegin("cupsJSONGetType(root)");
    type = cupsJSONGetType(json);
    testEndMessage(type == CUPS_JTYPE_OBJECT, "%s", types[type]);

    testBegin("cupsJSONNewKey('string')");
    current = cupsJSONNewKey(json, NULL, "string");
    testEnd(current != NULL);

    testBegin("cupsJSONGetType(key)");
    type = cupsJSONGetType(current);
    testEndMessage(type == CUPS_JTYPE_KEY, "%s", types[type]);

    testBegin("cupsJSONNewString('value')");
    current = cupsJSONNewString(json, current, "value");
    testEnd(current != NULL);

    testBegin("cupsJSONGetType(string)");
    type = cupsJSONGetType(current);
    testEndMessage(type == CUPS_JTYPE_STRING, "%s", types[type]);

    testBegin("cupsJSONNewKey('number')");
    current = cupsJSONNewKey(json, NULL, "number");
    testEnd(current != NULL);

    testBegin("cupsJSONNewNumber(42)");
    current = cupsJSONNewNumber(json, current, 42);
    testEnd(current != NULL);

    testBegin("cupsJSONGetType(number)");
    type = cupsJSONGetType(current);
    testEndMessage(type == CUPS_JTYPE_NUMBER, "%s", types[type]);

    testBegin("cupsJSONNewKey('null')");
    current = cupsJSONNewKey(json, NULL, "null");
    testEnd(current != NULL);

    testBegin("cupsJSONNew(null)");
    current = cupsJSONNew(json, current, CUPS_JTYPE_NULL);
    testEnd(current != NULL);

    testBegin("cupsJSONGetType(null)");
    type = cupsJSONGetType(current);
    testEndMessage(type == CUPS_JTYPE_NULL, "%s", types[type]);

    testBegin("cupsJSONNewKey('false')");
    current = cupsJSONNewKey(json, NULL, "false");
    testEnd(current != NULL);

    testBegin("cupsJSONNew(false)");
    current = cupsJSONNew(json, current, CUPS_JTYPE_FALSE);
    testEnd(current != NULL);

    testBegin("cupsJSONGetType(false)");
    type = cupsJSONGetType(current);
    testEndMessage(type == CUPS_JTYPE_FALSE, "%s", types[type]);

    testBegin("cupsJSONNewKey('true')");
    current = cupsJSONNewKey(json, NULL, "true");
    testEnd(current != NULL);

    testBegin("cupsJSONNew(true)");
    current = cupsJSONNew(json, current, CUPS_JTYPE_TRUE);
    testEnd(current != NULL);

    testBegin("cupsJSONGetType(true)");
    type = cupsJSONGetType(current);
    testEndMessage(type == CUPS_JTYPE_TRUE, "%s", types[type]);

    testBegin("cupsJSONNewKey('array')");
    current = cupsJSONNewKey(json, NULL, "array");
    testEnd(current != NULL);

    testBegin("cupsJSONNew(array)");
    parent = cupsJSONNew(json, current, CUPS_JTYPE_ARRAY);
    testEnd(parent != NULL);

    testBegin("cupsJSONGetType(array)");
    type = cupsJSONGetType(parent);
    testEndMessage(type == CUPS_JTYPE_ARRAY, "%s", types[type]);

    testBegin("cupsJSONNewString(array, 'foo')");
    current = cupsJSONNewString(parent, NULL, "foo");
    testEnd(current != NULL);

    testBegin("cupsJSONNewString(array, 'bar')");
    current = cupsJSONNewString(parent, current, "bar");
    testEnd(current != NULL);

    testBegin("cupsJSONNewNumber(array, 0.5)");
    current = cupsJSONNewNumber(parent, current, 0.5);
    testEnd(current != NULL);

    testBegin("cupsJSONNewNumber(array, 123456789123456789.0)");
    current = cupsJSONNewNumber(parent, current, 123456789123456789.0);
    testEnd(current != NULL);

    testBegin("cupsJSONNew(array, null)");
    current = cupsJSONNew(parent, current, CUPS_JTYPE_NULL);
    testEnd(current != NULL);

    testBegin("cupsJSONNewKey('object')");
    current = cupsJSONNewKey(json, NULL, "object");
    testEnd(current != NULL);

    testBegin("cupsJSONNew(object)");
    parent = cupsJSONNew(json, current, CUPS_JTYPE_OBJECT);
    testEnd(parent != NULL);

    testBegin("cupsJSONNewKey(object, 'a')");
    current = cupsJSONNewKey(parent, NULL, "a");
    testEnd(current != NULL);

    testBegin("cupsJSONNewString(object, 'one')");
    current = cupsJSONNewString(parent, current, "one");
    testEnd(current != NULL);

    testBegin("cupsJSONNewKey(object, 'b')");
    current = cupsJSONNewKey(parent, current, "b");
    testEnd(current != NULL);

    testBegin("cupsJSONNewNumber(object, 2)");
    current = cupsJSONNewNumber(parent, current, 2);
    testEnd(current != NULL);

    testBegin("cupsJSONGetCount(root)");
    count = cupsJSONGetCount(json);
    testEndMessage(count == 14, "%u", (unsigned)count);

    testBegin("cupsJSONSaveFile(root, 'test.json')");
    if (cupsJSONSaveFile(json, "test.json"))
    {
      testEnd(true);

      testBegin("cupsJSONLoadFile('test.json')");
      parent = cupsJSONLoadFile("test.json");
      testEnd(parent != NULL);

      cupsJSONDelete(parent);
    }
    else
    {
      testEndMessage(false, "%s", cupsLastErrorString());
    }

    testBegin("cupsJSONSaveString(root)");
    if ((s = cupsJSONSaveString(json)) != NULL)
    {
      testEnd(true);

      testBegin("cupsJSONLoadString('%s')", s);
      parent = cupsJSONLoadString(s);
      testEnd(parent != NULL);

      cupsJSONDelete(parent);
      free(s);
    }
    else
    {
      testEndMessage(false, "%s", cupsLastErrorString());
    }

    testBegin("cupsJSONDelete(root)");
    cupsJSONDelete(json);
    testEnd(true);

    testBegin("cupsJSONLoadURL('https://accounts.google.com/.well-known/openid-configuration', no last modified)");
    json = cupsJSONLoadURL("https://accounts.google.com/.well-known/openid-configuration", &last_modified);

    if (json)
    {
      char	last_modified_date[256];// Last-Modified string value

      testEnd(true);
      cupsJSONDelete(json);

      testBegin("cupsJSONLoadURL('https://accounts.google.com/.well-known/openid-configuration', since %s)", httpGetDateString(last_modified, last_modified_date, sizeof(last_modified_date)));
      json = cupsJSONLoadURL("https://accounts.google.com/.well-known/openid-configuration", &last_modified);

      if (json)
        testEnd(true);
      else if (cupsLastError() == IPP_STATUS_OK_EVENTS_COMPLETE)
        testEndMessage(true, "no change from last request");
      else
        testEndMessage(false, cupsLastErrorString());

      cupsJSONDelete(json);
    }
    else if (cupsLastError() == IPP_STATUS_ERROR_SERVICE_UNAVAILABLE)
    {
      testEndMessage(true, "%s", cupsLastErrorString());
    }
    else
    {
      testEndMessage(false, "%s", cupsLastErrorString());
    }
#endif // 0

    if (!testsPassed)
      return (1);
  }
  else
  {
    // Try loading JWT string on the command-line...
    for (i = 1; i < argc; i ++)
    {
      if ((jwt = cupsJWTImportString(argv[i])) != NULL)
      {
//	printf("%s: OK, %u key/value pairs in root object.\n", argv[i], (unsigned)(cupsJSONGetCount(json) / 2));

        cupsJWTDelete(jwt);
      }
      else
      {
	fprintf(stderr, "%s: %s\n", argv[i], cupsLastErrorString());
	return (1);
      }
    }
  }

  return (0);
}
