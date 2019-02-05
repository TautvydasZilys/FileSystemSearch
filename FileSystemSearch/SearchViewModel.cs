using System;
using System.ComponentModel;
using System.IO;

namespace FileSystemSearch
{
	public enum ByteUnit
	{
		B,
		KB,
		MB,
		GB,
		TB
	}

	public class SearchViewModel : INotifyPropertyChanged
	{
		private ulong ignoreFilesLargerThanInBytes;
		private ByteUnit[] byteUnits = new ByteUnit[] { ByteUnit.B, ByteUnit.KB, ByteUnit.MB, ByteUnit.GB, ByteUnit.TB };

		public event PropertyChangedEventHandler PropertyChanged;

		public SearchViewModel()
		{
			SelectedIgnoreFilesLargerThanByteUnit = ByteUnit.MB;
			ignoreFilesLargerThanInBytes = 10 * 1024 * 1024;
			SearchPattern = "*";

			SearchPath = "C:";
			SearchString = "";
			SearchForFiles = true;
			SearchInFileName = true;
			SearchRecursively = true;
			SearchIgnoreCase = true;
            SearchIgnoreDotStart = true;
		}

		public string SearchPath { get; set; }

		public string SearchPattern { get; set; }
		public string SearchString { get; set; }

		public string SearchPathAndPattern
		{
			get
			{
				// Path.Combine fails to combine "C:" with "*.cpp", producing "C:*.cpp"
				var result = SearchPath;
				if (SearchPath[SearchPath.Length - 1] != '\\')
					result += '\\';

				return result + SearchPattern;
			}
		}

		public bool SearchForFiles { get; set; }
		public bool SearchInFilePath { get; set; }
		public bool SearchInFileName { get; set; }
		public bool SearchInFileContents { get; set; }
		public bool SearchContentsAsUtf8 { get; set; }
		public bool SearchContentsAsUtf16 { get; set; }

		public bool SearchForDirectories { get; set; }
		public bool SearchInDirectoryPath { get; set; }
		public bool SearchInDirectoryName { get; set; }

		public bool SearchRecursively { get; set; }
		public bool SearchIgnoreCase { get; set; }
		public bool SearchIgnoreDotStart { get; set; }

		public double IgnoreFilesLargerThan
		{
			get
			{
				switch (SelectedIgnoreFilesLargerThanByteUnit)
				{
					case ByteUnit.B:
						return (double)ignoreFilesLargerThanInBytes;

					case ByteUnit.KB:
						return ignoreFilesLargerThanInBytes / 1024.0;

					case ByteUnit.MB:
						return ignoreFilesLargerThanInBytes / 1024.0 / 1024.0;

					case ByteUnit.GB:
						return ignoreFilesLargerThanInBytes / 1024.0 / 1024.0 / 1024.0;

					case ByteUnit.TB:
						return ignoreFilesLargerThanInBytes / 1024.0 / 1024.0 / 1024.0 / 1024.0;

					default:
						throw new Exception(string.Format("Unknown ByteUnit: ", SelectedIgnoreFilesLargerThanByteUnit));
				}
			}
			set
			{
				switch (SelectedIgnoreFilesLargerThanByteUnit)
				{
					case ByteUnit.B:
						ignoreFilesLargerThanInBytes = (ulong)value;
						break;

					case ByteUnit.KB:
						ignoreFilesLargerThanInBytes = (ulong)(value * 1024.0);
						break;

					case ByteUnit.MB:
						ignoreFilesLargerThanInBytes = (ulong)(value * 1024.0 * 1024.0);
						break;

					case ByteUnit.GB:
						ignoreFilesLargerThanInBytes = (ulong)(value * 1024.0 * 1024.0 * 1024.0);
						break;

					case ByteUnit.TB:
						ignoreFilesLargerThanInBytes = (ulong)(value * 1024.0 * 1024.0 * 1024.0 * 1024.0);
						break;

					default:
						throw new Exception(string.Format("Unknown ByteUnit: ", SelectedIgnoreFilesLargerThanByteUnit));
				}
			}
		}

		public ulong IgnoreFilesLargerThanInBytes { get { return ignoreFilesLargerThanInBytes; } }

		private ByteUnit selectedIgnoreFliesLargerThanByteUnit;
		public ByteUnit SelectedIgnoreFilesLargerThanByteUnit
		{
			get { return selectedIgnoreFliesLargerThanByteUnit; }
			set
			{
				selectedIgnoreFliesLargerThanByteUnit = value;
				OnPropertyChanged("IgnoreFilesLargerThan");
			}
		}

		public ByteUnit[] ByteUnits { get { return byteUnits; } }

		protected void OnPropertyChanged(string name)
		{
			PropertyChangedEventHandler handler = PropertyChanged;
			if (handler != null)
			{
				handler(this, new PropertyChangedEventArgs(name));
			}
		}
	}
}
