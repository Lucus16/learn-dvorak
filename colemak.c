#include <sys/time.h>
#include <termios.h>
#include <stdbool.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

// DONE: Fix new letters appearing infrequently, this was due to weight being
// mostly determined by word weight
// TODO: Arithmetic average
// TODO: Repeat the most inaccurate word several times
// TODO: Accuracy tracking for two- and three-letter combinations
// TODO: Accuracy tracking for whole words

typedef uint64_t Time;
typedef uint64_t Chance;

typedef struct Word {
	char *word;
	int hardest_letter;
	size_t len;
	Chance chance;
	float accuracy;
} Word;

static const Time MAX_TIME = 1000;
static const char ORDER[26] = "etaoinshrdlucmfwypvbgkqjxz";
static const int ORDER_POS[26] = {
	2, 19, 12, 9, 0, 14, 20, 7, 4, 23, 21, 10, 13,
	5, 3, 17, 22, 8, 6, 1, 11, 18, 15, 24, 16, 25
};

float key_accuracy[26] = {
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
};
Word *letter_start[27];
int unlocked = 2;
Word *words;
size_t word_count;
char line[81];
Word *line_word[80];
size_t line_len;
Chance chance_sum;

struct termios initial_term_settings;
void start() {
	struct termios term;
	tcgetattr(fileno(stdin), &initial_term_settings);
	tcgetattr(fileno(stdin), &term);
	term.c_lflag &= ~ICANON;
	term.c_lflag &= ~ECHO;
	tcsetattr(fileno(stdin), TCSANOW, &term);
}

void finish() {
	tcsetattr(fileno(stdin), TCSANOW, &initial_term_settings);
	printf("\nYou unlocked %i letters.\n", unlocked);
	exit(0);
}

int word_hardest_letter(char *w) {
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
		cur_word->len = c - cur_word->word;
		cur_word->hardest_letter = word_hardest_letter(cur_word->word);
		// Start at perfect word accuracy so only hard words are learnt.
		cur_word->accuracy = 0.2;
		cur_word++;
	}

	// Sort words and set up letter_start
	qsort(words, word_count, sizeof(Word), compare_words);
	int j = 0;
	for (size_t i = 0; i < word_count; i++) {
		if (words[i].hardest_letter == j) {
			letter_start[j] = &words[i];
			j++;
		}
	}
	letter_start[26] = words + word_count;
}

void print_word_scores() {
	for (Word *w = words; w < letter_start[unlocked]; w++) {
		printf("%15li %s\n", w->chance, w->word);
	}
}

Time now_ms() {
	struct timeval t;
	gettimeofday(&t, NULL);
	Time ms = t.tv_sec * 1000 + t.tv_usec / 1000;
	return ms;
}

Time prev_time;
Time avg_time;
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

Word *currently_typed_word;
float current_word_accuracy;
void update_accuracy(char pressed, char target, Time time, Word *word) {
	bool correct = (pressed == target);
	int score = correct ? time : MAX_TIME;
	if (target >= 'a' && target <= 'z') {
		key_accuracy[target - 'a'] = key_accuracy[target - 'a'] * 0.95 + score * 0.00005;
	}
	if (pressed >= 'a' && pressed <= 'z') {
		key_accuracy[pressed - 'a'] = key_accuracy[pressed - 'a'] * 0.95 + score * 0.00005;
	}
	if (word) {
		currently_typed_word = word;
		current_word_accuracy += score * 0.001;
	} else {
		currently_typed_word->accuracy = currently_typed_word->accuracy * 0.9 +
			(current_word_accuracy / currently_typed_word->len) * 0.1;
		current_word_accuracy = 0.0;
		currently_typed_word = NULL;
	}
}

Chance rate_word(Word *w) {
	float total = 1.0;
	for (char *c = w->word; c < w->word + w->len; c++) {
		float tmp = key_accuracy[*c - 'a'];
		total *= tmp * tmp;
	}
	return pow(total, 1.0 / w->len) * 0x10000;
}

Word *random_word() {
	Chance r = (((Chance)rand() << 32) | (Chance)rand()) % chance_sum;
	Word *w = words;
	while (r >= w->chance) {
		r -= (w++)->chance;
	}
	return w;
}

void generate_line() {
	// Generate weights
	chance_sum = 0;
	for (Word *w = words; w < letter_start[unlocked]; w++) {
		w->chance = rate_word(w);
		chance_sum += w->chance;
	}

#ifdef DEBUG
	for (Word *w = letter_start[0]; w < letter_start[unlocked]; w++) {
		printf("%8li %s\n", w->chance, w->word);
	}
#endif // DEBUG

	//print_word_scores();
	line_len = 0;
	line[0] = '\0';
	while (line_len < 72) {
		Word *w = random_word();
		for (size_t i = line_len; i < line_len + w->len; i++) {
			line_word[i] = w;
		}
		if (line_len + w->len + 1 > 80) {
			continue;
		}
		line_len += w->len + 1;
		line_word[line_len - 1] = NULL;
		strcat(line, w->word);
		strcat(line, " ");
		// Select weighted random word
	}
	line_len--;
	line[line_len] = '\0';
}

void run_line() {
	size_t pos = 0;
	int accurate = 0;
	Time line_time = 0;
	printf("%s\n", line);
	key_time();
	currently_typed_word = NULL;
	current_word_accuracy = 0.0;

	while (true) {
		char c = getchar();
		Time t = key_time();
		line_time += t;
		if (c == '\n' || (pos >= line_len && c == ' ')) {
			break;
		} else if (c == -1 || c == 4) {
			finish();
		} else if (c == ' ' || (c >= 'a' && c <= 'z')) {
			printf("%c", c);
			if (pos < line_len) {
				update_accuracy(c, line[pos], t, line_word[pos]);
				if (c == line[pos]) { accurate++; }
			} else {
				update_accuracy(c, ' ', t, NULL);
			}
		}
		pos++;
	}

	int wpm = 12000 * pos / line_time;
	float accuracy = 1.0 * accurate / line_len;
	printf("\n\n%3i WPM, %5.1f%% accuracy\n\n", wpm, 100.0 * accuracy);
	if (accuracy > 0.999 && wpm > 32 && unlocked < 26) {
		unlocked++;
	}
#ifdef DEBUG
	for (int i = 0; i < unlocked; i++) {
		printf("%c %f\n", ORDER[i], key_accuracy[ORDER[i] - 'a']);
	}
#endif // DEBUG
}

int main() {
	srand(now_ms());
	read_wordlist();
	start();
	avg_time = MAX_TIME;
	key_time();
	while (true) {
		generate_line();
		run_line();
	}
}
