# Installation #

1. Move the whole DropboxFilter directory to C:\.
2. Copy the DropboxFilter.cfg to the root of your Dropbox folder and adjust it
   to your needs.
3. Run the Install.bat script as an administrator.
	- This will first create a backup of the registry entries that are going to
	  be changed.
	- If you are asked to overwrite entries answer with yes (Y then enter).
4. Restart the Dropbox application. When you now add or change a file that is
   listed in DropboxFilter.cfg you should see that Dropbox does not even attempt
   to sync it.
