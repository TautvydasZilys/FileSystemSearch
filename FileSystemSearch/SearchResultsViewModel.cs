using System.ComponentModel;
using System.Runtime.CompilerServices;

namespace FileSystemSearch
{
	public class SearchResultsViewModel : INotifyPropertyChanged
	{
		private ulong directoriesEnumerated;
		private ulong filesEnumerated;
		private ulong fileContentsSearched;
		private ulong totalEnumeratedFilesSizeInBytes;
		private ulong totalContentSearchedFilesSizeInBytes;
		private ulong resultsFound;
		private double searchTimeInSeconds;

		public event PropertyChangedEventHandler PropertyChanged;

		public SearchResultsViewModel()
		{
		}

		public ulong DirectoriesEnumerated
		{
			get { return directoriesEnumerated; }
			set
			{
				directoriesEnumerated = value;
				OnPropertyChanged();
			}
		}

		public ulong FilesEnumerated
		{
			get { return filesEnumerated; }
			set
			{
				filesEnumerated = value;
				OnPropertyChanged();
			}
		}

		public ulong FileContentsSearched
		{
			get { return fileContentsSearched; }
			set
			{
				fileContentsSearched = value;
				OnPropertyChanged();
			}
		}

		public ulong TotalEnumeratedFilesSizeInBytes
		{
			get { return totalEnumeratedFilesSizeInBytes; }
			set
			{
				totalEnumeratedFilesSizeInBytes = value;
				OnPropertyChanged();
				OnPropertyChanged("TotalEnumeratedFilesSize");
            }
		}

		public ulong TotalContentSearchedFilesSizeInBytes
		{
			get { return totalContentSearchedFilesSizeInBytes; }
			set
			{
				totalContentSearchedFilesSizeInBytes = value;
				OnPropertyChanged();
				OnPropertyChanged("TotalContentSearchedFilesSize");
			}
		}

		public string TotalEnumeratedFilesSize { get { return FormatFileSize(totalEnumeratedFilesSizeInBytes); } }

		public string TotalContentSearchedFilesSize { get { return FormatFileSize(totalContentSearchedFilesSizeInBytes); } }

		public ulong ResultsFound
		{
			get
			{
				return resultsFound;
			}
			set
			{
				resultsFound = value;
				OnPropertyChanged();
			}
		}

		public double SearchTimeInSeconds
		{
			get
			{
				return searchTimeInSeconds;
			}
			set
			{
				searchTimeInSeconds = value;
				OnPropertyChanged();
				OnPropertyChanged("SearchTime");
			}
		}

		public string SearchTime
		{
			get
			{
				return string.Format("{0:0.###} seconds", SearchTimeInSeconds);
			}
		}

		private string FormatFileSize(double size)
		{
			if (size < 1024)
				return string.Format("{0} bytes", size);

			size /= 1024.0;

			if (size < 1024)
				return string.Format("{0:0.###} KB", size);

			size /= 1024.0;

			if (size < 1024)
				return string.Format("{0:0.###} MB", size);

			size /= 1024.0;

			if (size < 1024)
				return string.Format("{0:0.###} GB", size);

			size /= 1024.0;
			return string.Format("{0:0.###} TB", size);
		}

		protected void OnPropertyChanged([CallerMemberName]string name = null)
		{
			PropertyChangedEventHandler handler = PropertyChanged;
			if (handler != null)
			{
				handler(this, new PropertyChangedEventArgs(name));
			}
		}
	}
}
