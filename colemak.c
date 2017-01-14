#include <sys/time.h>
#include <termios.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

typedef uint64_t Time;
typedef uint64_t Chance;

typedef struct Word {
	char *word;
	int hardest_letter;
	Chance chance;
} Word;

static const int MAX_TIME = 1000;
// static const char ORDER[26] = "etaoinshrdlucmfwypvbgkqjxz";
static const int ORDER_POS[26] = {
	2, 19, 12, 9, 0, 14, 20, 7, 4, 23, 21, 10, 13,
	5, 3, 17, 22, 8, 6, 1, 11, 18, 15, 24, 16, 25
};

int key_accuracy[26];
int global_accuracy;
Word *letter_start[26];
int unlocked = 2;
Word *words;
size_t word_count;
char line[80];

int value_word(char *w) {
	int v = 0;
	for (char *c = w; *c; c++) {
		int i = ORDER_POS[*c - 'a'];
		if (i > v) { v = i; }
	}
	return v;
}

int compare_words(const void *a, const void *b) {
	return ((Word*)a)->hardest_letter - ((Word*)b)->hardest_letter;
}

void update_accuracy(char pressed, char target, Time time, Word word) {
}

void generate_line() {
}

void filter_wordlist() {
}

void read_wordlist() {
	// Read wordlist
	char *buff = malloc((1 << 20) * sizeof(char));
	FILE *fp = fopen("wordlist", "r");
	char *end = buff + fread(buff, sizeof(char), 1 << 20, fp);
	fclose(fp);
	// Count words
	word_count = 0;
	for (char *c = buff; c < end; c++) {
		if (*c == '\n') {
			word_count++;
		}
	}
	// Store words
	words = malloc(word_count * sizeof(Word));
	Word *cur_word = words;
	for (char *c = buff; c < end; c++) {
		cur_word->word = c;
		while (*c != '\n') { c++; }
		*c = '\0';
		cur_word->hardest_letter = value_word(cur_word->word);
		cur_word++;
	}
	// Sort words and set up letter_start
	qsort(words, word_count, sizeof(Word), compare_words);
	int j = 0;
	for (int i = 0; i < word_count; i++) {
		if (words[i].hardest_letter == j) {
			letter_start[j] = &words[i];
			j++;
		}
	}
}

Word random_word() {
	return words[rand() % word_count];
}

int now_ms() {
	struct timeval t;
	gettimeofday(&t, NULL);
	Time ms = t.tv_sec * 1000 + t.tv_usec / 1000;
	return ms;
}

int prev_time;
int avg_time;
Time key_time() {
	Time now = now_ms();
	Time diff = now - prev_time;
	prev_time = now;
	if (diff > MAX_TIME) {
		diff = MAX_TIME;
	}
	if (diff > 0) {
		avg_time = (diff + 15 * avg_time) / 16;
	}
	return diff;
}

struct termios term;
void start() {
	avg_time = MAX_TIME;
	tcgetattr(fileno(stdin), &term);
	term.c_lflag &= ~ICANON;
	term.c_lflag &= ~ECHO;
	tcsetattr(fileno(stdin), TCSANOW, &term);
}

void stop() {
	term.c_lflag |= ICANON;
	term.c_lflag |= ECHO;
	tcsetattr(fileno(stdin), TCSANOW, &term);
	printf("\n");
}

int main() {
	read_wordlist();
	start();
	char c = 0;
	while (c != 4) {
		c = getchar();
		printf("%c\t%5lims\n", c, key_time());
	}
	stop();
	return 0;
}
