
static struct rchan *relayfsOutput;
static struct dentry *relayfsDir = NULL;

static struct dentry *create_buf_file_handler(const char *filename,
	struct dentry *parent,
	umode_t mode,
	struct rchan_buf *buf,
	int *is_global) {

	return debugfs_create_file(filename, 0777, parent, buf,&relay_file_operations);
}

static int remove_buf_file_handler(struct dentry *dentry) {

	debugfs_remove(dentry);
	return 0;
}

static struct rchan_callbacks relay_callbacks = {
	.create_buf_file = create_buf_file_handler,
	.remove_buf_file = remove_buf_file_handler,
};

static int initRelayFS(void) {
	relayfsDir = debugfs_create_dir(RELAYFS_DIR,NULL);
	if (relayfsDir == NULL) {
		ERR_MSG("Cannot create debugfs direcotry %s\n",RELAYFS_DIR);
		return -1;
	}
	relayfsOutput = relay_open(RELAYFS_NAME, relayfsDir, SUBBUF_SIZE, N_SUBBUFS,&relay_callbacks,NULL);
	if (relayfsOutput == NULL) {
		ERR_MSG("Cannot create relayfs %s\n",RELAYFS_NAME);
		debugfs_remove(relayfsDir);
		return -1;
	}
	return 0;
}

static void destroyRelayFS(void) {
	relay_close(relayfsOutput);
	debugfs_remove(relayfsDir);
}
