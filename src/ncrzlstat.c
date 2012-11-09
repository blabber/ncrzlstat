/*-
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <tobias.rehbein@web.de> wrote this file. As long as you retain this notice
 * you can do whatever you want with this stuff. If we meet some day, and you
 * think this stuff is worth it, you can buy me a beer in return.
 *                                                             Tobias Rehbein
 */

#include <assert.h>
#include <curses.h>
#include <errno.h>
#include <jansson.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include <sys/param.h>
#include <stdio.h>
#include <fetch.h>

#define URL "http://status.raumzeitlabor.de/api/full.json"

struct model {
	bool door;
	int devices;
	int members;
	time_t time;
	char **membernames;
};

void		 usage(void);
FILE		*fetch(const char *url);
struct model	*parse_model(FILE *status);
void		 free_model(struct model *model);
void		 init_curses(void);
void		 deinit_curses(void);
void		 display(struct model *model);
int		 namecmp(const void *name1, const void *name2);

int
main(int argc, char *argv[])
{
	int ch;
	while ((ch = getopt(argc, argv, "h")) != -1) {
		switch (ch) {
		case 'h':
			usage();
		}
	}

	init_curses();
	atexit(&deinit_curses);

	do {
		FILE *status = fetch(URL);
		assert(status != NULL);

		struct model *model = parse_model(status);
		assert(model != NULL);

		display(model);

		fclose(status);
		free_model(model);

	} while (getch() != 'q');

	return (0);
}

void
display(struct model *model)
{
	clear();

	attrset(A_NORMAL);
	mvaddstr(0, 0, "RaumZeitStatus");
	move(0, COLS - 24);
	addstr(ctime(&(model->time)));

	mvaddstr(2, 0, "Status:  ");
	attron(A_BOLD);
	if (model->door) {
		attron(COLOR_PAIR(1));
		addstr("open");
	} else {
		attron(COLOR_PAIR(2));
		addstr("closed");
	}
	attrset(A_NORMAL);

	mvaddstr(3, 0, "Devices: ");
	attron(A_BOLD);
	printw("%d", model->devices);
	attrset(A_NORMAL);

	mvaddstr(4, 0, "Present: ");
	attron(A_BOLD);
	printw("%d", model->members);
	attrset(A_NORMAL);

	int x = 20;
	size_t xoff = 0;
	for (int i = 0; i < model->members; i++) {
		int yoff = (i % (LINES - 2));
		int y = yoff + 2;
		if (i > 0 && yoff == 0)
			x += xoff + 2;

		size_t namelen = strlen(model->membernames[i]);
		if (xoff < namelen)
			xoff = namelen;

		mvprintw(y, x, "%s", model->membernames[i]);
	}

	refresh();
}

void
init_curses(void)
{
	initscr();
	cbreak();
	noecho();
	timeout(10000);

	if (has_colors()) {
		start_color();

		init_pair(1, COLOR_GREEN, COLOR_BLACK);
		init_pair(2, COLOR_RED, COLOR_BLACK);
	}
}

void
deinit_curses(void)
{
	endwin();
}

struct model *
parse_model(FILE *status)
{
	assert(status != NULL);

	json_error_t error;
	json_t *json = json_loadf(status, 0, &error);
	if (json == NULL) {
		fprintf(stderr, "Could not parse status: %s\n", error.text);
		exit(EXIT_FAILURE);
	}

	json_t *details = json_object_get(json, "details");
	if (!json_is_object(details)) {
		fprintf(stderr, "details is not an object\n");
		exit(EXIT_FAILURE);
	}

	struct model *model = malloc(sizeof(struct model));
	if (model == NULL) {
		fprintf(stderr, "Could not allocate memory for model: %s\n",
		    strerror(errno));
		exit(EXIT_FAILURE);
	}

	model->time = time(NULL);

	json_t *obj;
	obj = json_object_get(details, "tuer");
	if (!json_is_string(obj)) {
		fprintf(stderr, "tuer is not a string\n");
		exit(EXIT_FAILURE);
	}
	model->door = strcmp("1", json_string_value(obj)) == 0 ? true : false;

	obj = json_object_get(details, "geraete");
	if (!json_is_integer(obj)) {
		fprintf(stderr, "geraete is not an integer\n");
		exit(EXIT_FAILURE);
	}
	model->devices = json_integer_value(obj);

	obj = json_object_get(details, "laboranten");
	if (!json_is_array(obj)) {
		fprintf(stderr, "laboranten is not an array\n");
		exit(EXIT_FAILURE);
	}
	model->members = json_array_size(obj);

	model->membernames = malloc(model->members * sizeof(char *));
	if (model->membernames == NULL) {
		fprintf(stderr, "Could not allocate membernames: %s\n",
		    strerror(errno));
	}
	for (int i = 0; i < model->members; i++) {
		json_t *m = json_array_get(obj, i);
		if (!json_is_string(m)) {
			fprintf(stderr, "member is not a string\n");
			exit(EXIT_FAILURE);
		}
		model->membernames[i] = strdup(json_string_value(m));
		if (model->membernames[i] == NULL) {
			fprintf(stderr, "Could not strdup knownmember: %s\n",
			    strerror(errno));
			exit(EXIT_FAILURE);
		}
	}
	qsort(model->membernames, model->members, sizeof(char**),  &namecmp);

	json_decref(json);

	return (model);
}

void
free_model(struct model *model)
{
	assert(model != NULL);

	for (int i = 0; i < model->members; i++) {
		free(model->membernames[i]);
	}

	free(model);
}

FILE *
fetch(const char *url)
{
	assert(url != NULL);

	FILE *f = fetchGetURL(url, "");
	if (f == NULL) {
		fprintf(stderr, "Could not get URL \"%s\"\n", url);
		exit(EXIT_FAILURE);
	}

	return (f);
}

void
usage(void)
{
	fprintf(stderr, "%s has no options.\n", getprogname());
}

int
namecmp(const void *name1, const void *name2)
{
	const char *c1 = *(char * const *)name1;
	const char *c2 = *(char * const *)name2;

	return strcmp(c1, c2);
}
