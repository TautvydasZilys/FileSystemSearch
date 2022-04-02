﻿using System.Linq;
using System.Windows;
using System.Windows.Controls;

namespace FileSystemSearch
{
	public partial class SearchWindow : Window
	{
		SearchViewModel viewModel;

		public SearchWindow()
		{
			InitializeComponent();
			searchStringTextBox.Focus();

			viewModel = new SearchViewModel();
			DataContext = viewModel;
		}

		static bool HasValidationErrors(DependencyObject obj)
		{
			return Validation.GetHasError(obj) || LogicalTreeHelper.GetChildren(obj).OfType<DependencyObject>().Any(HasValidationErrors);
		}

		private void ButtonSearch_Click(object sender, RoutedEventArgs e)
		{
			// This is needed because WPF bindings get only updated when textboxes lose focus
			// If this isn't done, then hitting search button with ENTER shortcut makes us not see the last textbox edit
			((Button)sender).Focus();

			if (HasValidationErrors(this))
			{
				MessageBox.Show("One or more of search parameters are invalid.", "Search is not possible", MessageBoxButton.OK, MessageBoxImage.Error);
				return;
			}

			string validationFailedReason;
			if (!SearchUtils.ValidateSearchViewModel(viewModel, out validationFailedReason))
			{
				MessageBox.Show(validationFailedReason, "Search is not possible", MessageBoxButton.OK, MessageBoxImage.Error);
				return;
			}

			var resultWindow = new SearchResultWindow(viewModel);
			resultWindow.Show();
		}
	}
}
