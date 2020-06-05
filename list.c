#include "config.h"
#include <dirent.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "maildir/maildir_private.h"
#include "mutt/lib.h"
#include "email/lib.h"
#include "progress.h"
#include "maildir/lib.h"

#undef USE_HCACHE

char *C_HeaderCache;
short C_Sort;
bool C_FlagSafe;
bool C_MaildirHeaderCacheVerify;
bool C_Autocrypt;
SIG_ATOMIC_VOLATILE_T SigInt;

#define INS_SORT_THRESHOLD 6

struct Maildir *maildir_edata_get(struct Email *e)
{
  if (!e)
    return NULL;
  return e->edata;
}

void mx_alloc_memory(struct Mailbox *m)
{
  size_t s = MAX(sizeof(struct Email *), sizeof(int));

  if ((m->email_max + 25) * s < m->email_max * s)
  {
    mutt_error(_("Out of memory"));
    mutt_exit(1);
  }

  m->email_max += 25;
  if (m->emails)
  {
    mutt_mem_realloc(&m->emails, sizeof(struct Email *) * m->email_max);
    mutt_mem_realloc(&m->v2r, sizeof(int) * m->email_max);
  }
  else
  {
    m->emails = mutt_mem_calloc(m->email_max, sizeof(struct Email *));
    m->v2r = mutt_mem_calloc(m->email_max, sizeof(int));
  }
  for (int i = m->email_max - 25; i < m->email_max; i++)
  {
    m->emails[i] = NULL;
    m->v2r[i] = -1;
  }
}

int mutt_autocrypt_process_autocrypt_header(struct Email *e, struct Envelope *env)
{
  if (e || env)
  {
  }
  return 0;
}

mode_t mh_umask(struct Mailbox *m)
{
  if (m)
  {
  }
  return 0;
}

bool mh_valid_message(const char *s)
{
  if (s)
  {
  }
  return false;
}

void mhs_sequences_free(struct MhSequences *mhs)
{
  if (mhs)
  {
  }
}

void mutt_progress_init(struct Progress *progress, const char *msg, enum ProgressType type, size_t size)
{
  if (progress || msg || type || size)
  {
  }
}

void mutt_progress_update(struct Progress *progress, size_t pos, int percent)
{
  if (progress || pos || percent)
  {
  }
}

int mh_read_sequences(struct MhSequences *mhs, const char *path)
{
  if (mhs || path)
  {
  }
  return 0;
}

void maildir_mdata_free(void **ptr)
{
  if (!ptr || !*ptr)
    return;

  // struct MaildirMboxData *mdata = *ptr;
  FREE(ptr);
}

struct MaildirMboxData *maildir_mdata_new(void)
{
  struct MaildirMboxData *mdata = mutt_mem_calloc(1, sizeof(struct MaildirMboxData));
  return mdata;
}

struct Maildir *maildir_ins_sort(struct Maildir *list, int (*cmp)(struct Maildir *, struct Maildir *))
{
  struct Maildir *tmp = NULL, *last = NULL, *back = NULL;

  struct Maildir *ret = list;
  list = list->next;
  ret->next = NULL;

  while (list)
  {
    last = NULL;
    back = list->next;
    for (tmp = ret; tmp && cmp(tmp, list) <= 0; tmp = tmp->next)
      last = tmp;

    list->next = tmp;
    if (last)
      last->next = list;
    else
      ret = list;

    list = back;
  }

  return ret;
}

struct Maildir * maildir_merge_lists(struct Maildir *left, struct Maildir *right, int (*cmp)(struct Maildir *, struct Maildir *))
{
  struct Maildir *head = NULL;
  struct Maildir *tail = NULL;

  if (left && right)
  {
    if (cmp(left, right) < 0)
    {
      head = left;
      left = left->next;
    }
    else
    {
      head = right;
      right = right->next;
    }
  }
  else
  {
    if (left)
      return left;
    return right;
  }

  tail = head;

  while (left && right)
  {
    if (cmp(left, right) < 0)
    {
      tail->next = left;
      left = left->next;
    }
    else
    {
      tail->next = right;
      right = right->next;
    }
    tail = tail->next;
  }

  if (left)
  {
    tail->next = left;
  }
  else
  {
    tail->next = right;
  }

  return head;
}

struct Maildir *maildir_sort(struct Maildir *list, size_t len, int (*cmp)(struct Maildir *, struct Maildir *))
{
  struct Maildir *left = list;
  struct Maildir *right = list;
  size_t c = 0;

  if (!list || !list->next)
  {
    return list;
  }

  if ((len != (size_t)(-1)) && (len <= INS_SORT_THRESHOLD))
    return maildir_ins_sort(list, cmp);

  list = list->next;
  while (list && list->next)
  {
    right = right->next;
    list = list->next->next;
    c++;
  }

  list = right;
  right = right->next;
  list->next = 0;

  left = maildir_sort(left, c, cmp);
  right = maildir_sort(right, c, cmp);
  return maildir_merge_lists(left, right, cmp);
}

int md_cmp_path(struct Maildir *a, struct Maildir *b)
{
  return strcmp(a->email->path, b->email->path);
}

void mh_sort_natural(struct Mailbox *m, struct Maildir **md)
{
  if (!m || !md || !*md || (m->type != MUTT_MH) || (C_Sort != SORT_ORDER))
    return;
  mutt_debug(LL_DEBUG3, "maildir: sorting %s into natural order\n", mailbox_path(m));
  *md = maildir_sort(*md, (size_t) -1, md_cmp_path);
}

struct Maildir *skip_duplicates(struct Maildir *p, struct Maildir **last)
{
  /* Skip ahead to the next non-duplicate message.
   *
   * p should never reach NULL, because we couldn't have reached this point
   * unless there was a message that needed to be parsed.
   *
   * The check for p->header_parsed is likely unnecessary since the dupes will
   * most likely be at the head of the list.  but it is present for consistency
   * with the check at the top of the for() loop in maildir_delayed_parsing().
   */
  while (!p->email || p->header_parsed)
  {
    *last = p;
    p = p->next;
  }
  return p;
}

int md_cmp_inode(struct Maildir *a, struct Maildir *b)
{
  return a->inode - b->inode;
}

void maildir_parse_flags(struct Email *e, const char *path)
{
  char *q = NULL;

  e->flagged = false;
  e->read = false;
  e->replied = false;

  char *p = strrchr(path, ':');
  if (p && mutt_str_startswith(p + 1, "2,", CASE_MATCH))
  {
    p += 3;

    mutt_str_replace(&e->maildir_flags, p);
    q = e->maildir_flags;

    while (*p)
    {
      switch (*p)
      {
        case 'F':
          e->flagged = true;
          break;

        case 'R': /* replied */
          e->replied = true;
          break;

        case 'S': /* seen */
          e->read = true;
          break;

        case 'T': /* trashed */
          if (!e->flagged || !C_FlagSafe)
          {
            e->trash = true;
            e->deleted = true;
          }
          break;

        default:
          *q++ = *p;
          break;
      }
      p++;
    }
  }

  if (q == e->maildir_flags)
    FREE(&e->maildir_flags);
  else if (q)
    *q = '\0';
}

struct Email *maildir_parse_stream(enum MailboxType type, FILE *fp, const char *fname, bool is_old, struct Email *e)
{
  if (!e)
    e = email_new();
  e->env = mutt_rfc822_read_header(fp, e, false, false);

  struct stat st;
  fstat(fileno(fp), &st);

  if (!e->received)
    e->received = e->date_sent;

  /* always update the length since we have fresh information available. */
  e->content->length = st.st_size - e->content->offset;

  e->index = -1;

  if (type == MUTT_MAILDIR)
  {
    /* maildir stores its flags in the filename, so ignore the
     * flags in the header of the message */

    e->old = is_old;
    maildir_parse_flags(e, fname);
  }
  return e;
}

struct Email *maildir_parse_message(enum MailboxType type, const char *fname, bool is_old, struct Email *e)
{
  FILE *fp = fopen(fname, "r");
  if (!fp)
    return NULL;

  e = maildir_parse_stream(type, fp, fname, is_old, e);
  mutt_file_fclose(&fp);
  return e;
}

void maildir_delayed_parsing(struct Mailbox *m, struct Maildir **md, struct Progress *progress)
{
  struct Maildir *p = NULL, *last = NULL;
  char fn[PATH_MAX];
  int count;
  bool sort = false;

#ifdef USE_HCACHE
  header_cache_t *hc = mutt_hcache_open(C_HeaderCache, mailbox_path(m), NULL);
#endif

  for (p = *md, count = 0; p; p = p->next, count++)
  {
    // printf("LAST = %p\n", (void *) last);
    if (!p || !p->email || p->header_parsed)
    {
      printf("SKIP: %s\n", p->canon_fname);
      last = p;
      continue;
    }

    if (m->verbose && progress)
      mutt_progress_update(progress, count, -1);

    if (!sort)
    {
      printf("SORT\n");
      mutt_debug(LL_DEBUG3, "maildir: need to sort %s by inode\n", mailbox_path(m));
      p = maildir_sort(p, (size_t) -1, md_cmp_inode);
      if (last)
        last->next = p;
      else
        *md = p;
      sort = true;
      p = skip_duplicates(p, &last);
      snprintf(fn, sizeof(fn), "%s/%s", mailbox_path(m), p->email->path);
    }

    snprintf(fn, sizeof(fn), "%s/%s", mailbox_path(m), p->email->path);

#ifdef USE_HCACHE
    struct stat lastchanged = { 0 };
    int rc = 0;
    if (C_MaildirHeaderCacheVerify)
    {
      rc = stat(fn, &lastchanged);
    }

    const char *key = NULL;
    size_t keylen = 0;
    if (m->type == MUTT_MH)
    {
      key = p->email->path;
      keylen = strlen(key);
    }
    else
    {
      key = p->email->path + 3;
      keylen = maildir_hcache_keylen(key);
    }
    struct HCacheEntry hce = mutt_hcache_fetch(hc, key, keylen, 0);

    if (hce.email && (rc == 0) && (lastchanged.st_mtime <= hce.uidvalidity))
    {
      hce.email->old = p->email->old;
      hce.email->path = mutt_str_strdup(p->email->path);
      email_free(&p->email);
      p->email = hce.email;
      if (m->type == MUTT_MAILDIR)
        maildir_parse_flags(p->email, fn);
    }
    else
#endif
    {
      printf("parse: %10lu %s\n", p->inode, fn);
      if (maildir_parse_message(m->type, fn, p->email->old, p->email))
      {
        p->header_parsed = 1;
#ifdef USE_HCACHE
        if (m->type == MUTT_MH)
        {
          key = p->email->path;
          keylen = strlen(key);
        }
        else
        {
          key = p->email->path + 3;
          keylen = maildir_hcache_keylen(key);
        }
        mutt_hcache_store(hc, key, keylen, p->email, 0);
#endif
      }
      else
        email_free(&p->email);
    }
    last = p;
  }
#ifdef USE_HCACHE
  mutt_hcache_close(hc);
#endif

  mh_sort_natural(m, md);
}

struct Maildir *maildir_edata_new(void)
{
  return mutt_mem_calloc(1, sizeof(struct Maildir));
}

void maildir_edata_free(void **ptr)
{
  if (!ptr || !*ptr)
    return;

  struct Maildir *md = *ptr;
  FREE(&md->canon_fname);

  FREE(ptr);
}

void maildir_free(struct Maildir **md)
{
  if (!md || !*md)
    return;

  struct Maildir *p = NULL, *q = NULL;

  for (p = *md; p; p = q)
  {
    q = p->next;
    maildir_edata_free((void *) &p);
  }
}

int maildir_move_to_mailbox(struct Mailbox *m, struct Maildir **ptr)
{
  if (!m)
    return 0;

  struct Maildir *md = *ptr;
  int oldmsgcount = m->msg_count;

  for (; md; md = md->next)
  {
    mutt_debug(LL_DEBUG2, "Considering %s\n", NONULL(md->canon_fname));
    if (!md->email)
      continue;

    mutt_debug(LL_DEBUG2, "Adding header structure. Flags: %s%s%s%s%s\n", md->email->flagged ? "f" : "", md->email->deleted ? "D" : "", md->email->replied ? "r" : "", md->email->old ? "O" : "", md->email->read ? "R" : "");
    if (m->msg_count == m->email_max)
      mx_alloc_memory(m);

    m->emails[m->msg_count] = md->email;
    m->emails[m->msg_count]->index = m->msg_count;
    mailbox_size_add(m, md->email);

    md->email = NULL;
    m->msg_count++;
  }

  int num = 0;
  if (m->msg_count > oldmsgcount)
    num = m->msg_count - oldmsgcount;

  maildir_free(ptr);
  return num;
}

int maildir_parse_dir(struct Mailbox *m, struct Maildir ***last, const char *subdir, int *count, struct Progress *progress)
{
  struct dirent *de = NULL;
  int rc = 0;
  bool is_old = false;
  struct Maildir *edata = NULL;
  struct Email *e = NULL;

  struct Buffer *buf = mutt_buffer_pool_get();

  if (subdir)
  {
    mutt_buffer_printf(buf, "%s/%s", mailbox_path(m), subdir);
    is_old = C_MarkOld ? (mutt_str_strcmp("cur", subdir) == 0) : false;
  }
  else
    mutt_buffer_strcpy(buf, mailbox_path(m));

  DIR *dirp = opendir(mutt_b2s(buf));
  if (!dirp)
  {
    rc = -1;
    goto cleanup;
  }

  while (((de = readdir(dirp))) && (SigInt != 1))
  {
    if (((m->type == MUTT_MH) && !mh_valid_message(de->d_name)) ||
        ((m->type == MUTT_MAILDIR) && (*de->d_name == '.')))
    {
      continue;
    }

    /* FOO - really ignore the return value? */
    mutt_debug(LL_DEBUG2, "queueing %s\n", de->d_name);

    e = email_new();
    e->old = is_old;
    if (m->type == MUTT_MAILDIR)
      maildir_parse_flags(e, de->d_name);

    if (count)
    {
      (*count)++;
      if (m->verbose && progress)
        mutt_progress_update(progress, *count, -1);
    }

    if (subdir)
    {
      mutt_buffer_printf(buf, "%s/%s", subdir, de->d_name);
      e->path = mutt_buffer_strdup(buf);
    }
    else
      e->path = mutt_str_strdup(de->d_name);

    edata = maildir_edata_new();
    edata->email = e;
    edata->inode = de->d_ino;
    **last = edata;
    *last = &edata->next;
  }

  closedir(dirp);

  if (SigInt == 1)
  {
    SigInt = 0;
    return -2; /* action aborted */
  }

cleanup:
  mutt_buffer_pool_release(&buf);

  return rc;
}

struct MaildirMboxData *maildir_mdata_get(struct Mailbox *m)
{
  if (!m || ((m->type != MUTT_MAILDIR) && (m->type != MUTT_MH)))
    return NULL;
  return m->mdata;
}

void maildir_update_mtime(struct Mailbox *m)
{
  char buf[PATH_MAX];
  struct stat st;
  struct MaildirMboxData *mdata = maildir_mdata_get(m);

  if (m->type == MUTT_MAILDIR)
  {
    snprintf(buf, sizeof(buf), "%s/%s", mailbox_path(m), "cur");
    if (stat(buf, &st) == 0)
      mutt_file_get_stat_timespec(&mdata->mtime_cur, &st, MUTT_STAT_MTIME);
    snprintf(buf, sizeof(buf), "%s/%s", mailbox_path(m), "new");
  }
  else
  {
    snprintf(buf, sizeof(buf), "%s/.mh_sequences", mailbox_path(m));
    if (stat(buf, &st) == 0)
      mutt_file_get_stat_timespec(&mdata->mtime_cur, &st, MUTT_STAT_MTIME);

    mutt_str_strfcpy(buf, mailbox_path(m), sizeof(buf));
  }

  if (stat(buf, &st) == 0)
    mutt_file_get_stat_timespec(&m->mtime, &st, MUTT_STAT_MTIME);
}

MhSeqFlags mhs_check(struct MhSequences *mhs, int i)
{
  if (!mhs->flags || (i > mhs->max))
    return 0;
  return mhs->flags[i];
}

void mh_update_maildir(struct Maildir *md, struct MhSequences *mhs)
{
  int i;

  for (; md; md = md->next)
  {
    char *p = strrchr(md->email->path, '/');
    if (p)
      p++;
    else
      p = md->email->path;

    if (mutt_str_atoi(p, &i) < 0)
      continue;
    MhSeqFlags flags = mhs_check(mhs, i);

    md->email->read = (flags & MH_SEQ_UNSEEN) ? false : true;
    md->email->flagged = (flags & MH_SEQ_FLAGGED) ? true : false;
    md->email->replied = (flags & MH_SEQ_REPLIED) ? true : false;
  }
}

int mh_read_dir(struct Mailbox *m, const char *subdir)
{
  if (!m)
    return -1;

  struct Maildir *md = NULL;
  struct MhSequences mhs = { 0 };
  struct Maildir **last = NULL;
  struct Progress progress;

  if (m->verbose)
  {
    char msg[PATH_MAX];
    snprintf(msg, sizeof(msg), _("Scanning %s..."), mailbox_path(m));
    mutt_progress_init(&progress, msg, MUTT_PROGRESS_READ, 0);
  }

  struct MaildirMboxData *mdata = maildir_mdata_get(m);
  if (!mdata)
  {
    mdata = maildir_mdata_new();
    m->mdata = mdata;
    m->mdata_free = maildir_mdata_free;
  }

  maildir_update_mtime(m);

  md = NULL;
  last = &md;
  int count = 0;
  if (maildir_parse_dir(m, &last, subdir, &count, &progress) < 0)
    return -1;

  if (m->verbose)
  {
    char msg[PATH_MAX];
    snprintf(msg, sizeof(msg), _("Reading %s..."), mailbox_path(m));
    mutt_progress_init(&progress, msg, MUTT_PROGRESS_READ, count);
  }
  maildir_delayed_parsing(m, &md, &progress);

  if (m->type == MUTT_MH)
  {
    if (mh_read_sequences(&mhs, mailbox_path(m)) < 0)
    {
      maildir_free(&md);
      return -1;
    }
    mh_update_maildir(md, &mhs);
    mhs_sequences_free(&mhs);
  }

  maildir_move_to_mailbox(m, &md);

  if (!mdata->mh_umask)
    mdata->mh_umask = mh_umask(m);

  return 0;
}

int main(int argc, char *argv[])
{
  const char *dir = "/home/mail/linode/neo";

  if (argc == 2)
    dir = argv[1];

  printf("reading: %s\n", dir);
  struct Mailbox *m = mailbox_new();
  m->type = MUTT_MAILDIR;

  mutt_buffer_strcpy(&m->pathbuf, dir);

  int rc = mh_read_dir(m, "cur");
  printf("rc = %d\n", rc);

  for (int i = 0; i < m->email_max; i++)
  {
    struct Email *e = m->emails[i];
    if (!e)
      continue;
    printf("%-30s %s\n", e->path, e->env->subject);
  }

  mailbox_free(&m);
  return 0;
}
