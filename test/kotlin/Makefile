VERSION = 1.2.0
JAR = kmp-negentropy-jvm-$(VERSION).jar
URL = https://repo1.maven.org/maven2/com/vitorpamplona/negentropy/kmp-negentropy-jvm/v$(VERSION)/kmp-negentropy-jvm-v$(VERSION).jar

all: $(JAR)

$(JAR):
	curl -sSfL -o $(JAR) $(URL)

clean:
	rm -f kmp-negentropy-jvm-*.jar

.PHONY: all clean
