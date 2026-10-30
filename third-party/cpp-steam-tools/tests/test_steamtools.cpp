// Tests for SteamTools behaviour that does not need a Steam install:
// launch-option quoting, and later the user resolution and write rules
// against a temporary Steam directory.

#include "steamtools.h"

#include <QCoreApplication>

#include <cstdio>

static int failures = 0;

#define CHECK(cond)                                                                 \
	do {                                                                            \
		if(!(cond)) {                                                               \
			fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);         \
			failures++;                                                             \
		}                                                                           \
	} while(0)

// Steam splits LaunchOptions like a shell, so a value with spaces has to
// arrive as one argument.
static void testQuoteArgument()
{
	CHECK(SteamTools::quoteArgument("Living Room") == QStringLiteral("\"Living Room\""));
	CHECK(SteamTools::quoteArgument("plain") == QStringLiteral("\"plain\""));
	CHECK(SteamTools::quoteArgument("") == QStringLiteral("\"\""));
	CHECK(SteamTools::quoteArgument("say \"hi\"") == QStringLiteral("\"say \\\"hi\\\"\""));
	CHECK(SteamTools::quoteArgument("back\\slash") == QStringLiteral("\"back\\\\slash\""));
}

int main(int argc, char **argv)
{
	QCoreApplication app(argc, argv);
	testQuoteArgument();
	if(failures)
	{
		fprintf(stderr, "%d check(s) failed\n", failures);
		return 1;
	}
	printf("steamtools: all checks passed\n");
	return 0;
}
