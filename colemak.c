#include <sys/time.h>
#include <termios.h>
#include <stdbool.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

// TODO: Need two lines to advance a letter
// TODO: Accuracy tracking for two- and three-letter combinations
// TODO: Better word list and better character order

typedef uint64_t Time;
typedef uint64_t Chance;
typedef float Accuracy;

typedef struct Word {
	char *word;
	int hardest_letter;
	size_t len;
	Chance chance;
	Accuracy accuracy;
} Word;

#ifdef DEBUG
static const char ORDER[26] = "etaoinshrdlucmfwypvbgkqjxz";
#endif // DEBUG

static const Time MAX_TIME = 1000;
static const int ORDER_POS[26] = {
	2, 19, 12, 9, 0, 14, 20, 7, 4, 23, 21, 10, 13,
	5, 3, 17, 22, 8, 6, 1, 11, 18, 15, 24, 16, 25
};

Accuracy key_accuracy[26] = {
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

Time now_ms() {
	struct timeval t;
	gettimeofday(&t, NULL);
	Time ms = t.tv_sec * 1000 + t.tv_usec / 1000;
	return ms;
}

Time key_time() {
	static Time prev_time;
	Time now = now_ms();
	Time diff = now - prev_time;
	prev_time = now;
	if (diff > MAX_TIME) {
		diff = MAX_TIME;
	}
	return diff;
}

//////////////// RATING ////////////////

void update_letter_accuracy(char letter, int score) {
	if (letter >= 'a' && letter <= 'z') {
		key_accuracy[letter - 'a'] = key_accuracy[letter - 'a'] * 0.95 + score * 0.00005;
	}
}

void update_word_accuracy(Word *word, int score) {
	if (word != NULL) {
		word->accuracy = word->accuracy * 0.5 + score * 0.0005;
	}
}

void update_accuracy(char pressed, char target, Time time, Word *word) {
	static Word *last_word = NULL;
	static size_t last_word_len;
	static size_t last_word_mistakes;
	static Accuracy last_word_accuracy;
	bool correct = (pressed == target);
	int score = correct ? time : MAX_TIME;

	update_letter_accuracy(pressed, score);
	update_letter_accuracy(target, score);

	if (last_word == NULL) {
		last_word = word;
		last_word_len = 1;
		last_word_mistakes = correct ? 0 : 1;
		last_word_accuracy = score;
	} else if (word == NULL) {
		last_word_mistakes += correct ? 0 : 1;
		update_word_accuracy(last_word, last_word_mistakes > 0 ? MAX_TIME :
				last_word_accuracy / last_word_len);
	} else {
		last_word_len += 1;
		last_word_mistakes += correct ? 0 : 1;
		last_word_accuracy += score;
	}
	last_word = word;
}

Chance rate_word(Word *w) {
	float total = 1.0;
	for (char *c = w->word; c < w->word + w->len; c++) {
		Accuracy tmp = key_accuracy[*c - 'a'];
		total += pow(26 * 26, tmp);
	}
	return 0x100 * (total / (w->len + 1)) *
		pow((letter_start[unlocked] - letter_start[0]), 2 * w->accuracy);
}

//////////////// RATING ////////////////

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

	line_len = 0;
	line[0] = '\0';
	while (line_len < 72) {
		Word *w = random_word();
		if (line_len + w->len + 1 > 80) {
			continue;
		}
		for (size_t i = line_len; i < line_len + w->len; i++) {
			line_word[i] = w;
		}
		//Chance half = w->chance >> 1;
		//chance_sum -= half;
		//w->chance -= half;
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
			pos++;
		}
	}
	update_accuracy(' ', ' ', 1.0, NULL);

	int wpm = 12000 * pos / line_time;
	int mistakes = line_len - accurate;
	printf("\n\n%3i WPM, %2i mistakes\n\n", wpm, mistakes);
	if (mistakes < 1 && wpm >= 32 && unlocked < 26) {
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
	while (true) {
		generate_line();
		run_line();
	}
}
