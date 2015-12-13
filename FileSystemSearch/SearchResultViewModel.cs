namespace FileSystemSearch
{
	public class SearchResultViewModel
	{
		public SearchResultViewModel(string path)
		{
			Path = path;
		}

		public string Path { get; set; }
	}
}
